#include <boost/json.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <random>
#include <mutex>
#include <fstream>

namespace json = boost::json;
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

const double HOURS_PER_STEP = 24.0 / 1000.0; // 1000 steps = 24 hours

// --- Node Types ---
enum class NodeType { Generator, Consumer, Battery };

// --- Node ---
struct Node {
    std::string name;
    NodeType type;
    double voltage;
    double load;        // negative = generation, positive = consumption
    double soc = 0.5;   // for batteries
    double capacity = 0.0; // battery capacity in kWh
    double minOutput = -1e6; // for generators
    double maxOutput = 1e6;  // for generators
    bool blackout = false;
};

// --- Line ---
struct Line {
    int from;
    int to;
    double reactance;
    double capacity;
    double loading = 0.0;
    double flow = 0.0;
};

// --- Network ---
struct Network {
    std::vector<Node> nodes;
    std::vector<Line> lines;
    std::mutex mtx;

    // --- Load network from JSON config ---
    void loadFromJSON(const std::string& filename) {
        std::ifstream file(filename);
        if(!file.is_open()) {
            std::cout << "Config file not found, using default network.\n";
            return;
        }

        try {
            std::string content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
            json::value jv = json::parse(content);
            json::object jobj = jv.as_object();

            nodes.clear();
            lines.clear();

            for (auto &n : jobj["nodes"].as_array()) {
                auto obj = n.as_object();
                Node node;
                node.name = obj["name"].as_string().c_str();
                std::string type = obj["type"].as_string().c_str();
                node.type = type == "Generator" ? NodeType::Generator :
                            type == "Consumer" ? NodeType::Consumer :
                            NodeType::Battery;
                node.voltage = obj["voltage"].as_double();
                node.load = obj["load"].as_double();
                node.soc = obj.contains("soc") ? obj["soc"].as_double() : 0.5;
                node.capacity = obj.contains("capacity") ? obj["capacity"].as_double() : 0.0;
                nodes.push_back(node);
            }

            for (auto &l : jobj["lines"].as_array()) {
                auto obj = l.as_object();
                Line line;
                line.from = obj["from"].as_int64();
                line.to = obj["to"].as_int64();
                line.reactance = obj["reactance"].as_double();
                line.capacity = obj["capacity"].as_double();
                lines.push_back(line);
            }

            std::cout << "Network loaded from " << filename << "\n";
        } catch(...) {
            std::cout << "Failed to parse config file, using default network.\n";
        }
    }

    void simulateStep() {
        std::lock_guard<std::mutex> lock(mtx);

        const double V_ref = 230.0;

        // --- 1. Reset voltages ---
        for (auto &n : nodes) n.voltage = V_ref;

        // --- 2. Battery Management ---
        double generation = 0.0;
        double consumption = 0.0;

        for (auto &n : nodes) {
            if (n.type == NodeType::Generator) generation += -n.load; // negative load = generation
            if (n.type == NodeType::Consumer) consumption += n.load;
        }

        double surplus = generation - consumption; // Überschuss erzeugt Batterie-Ladung

        for (auto &battery : nodes) {
            if (battery.type != NodeType::Battery) continue;

            double eff = 0.95;
            double maxCharge = 20.0;
            double maxDischarge = 20.0;

            if (surplus > 0) {
                double charge = std::min(maxCharge, surplus);
                battery.load = -charge;
                battery.soc += (charge * HOURS_PER_STEP * eff) / battery.capacity;
            } else {
                double demand = -surplus; // negative surplus = Defizit
                double discharge = std::min(maxDischarge, demand);
                battery.load = (battery.soc > 0.0) ? discharge : 0.0;
                battery.soc -= (discharge * HOURS_PER_STEP / eff) / battery.capacity;
            }

            battery.soc = std::clamp(battery.soc, 0.0, 1.0);
        }

        // --- 3. Directional power flow ---
        for (auto &l : lines) {
            double fromLoad = nodes[l.from].load;
            double toLoad = nodes[l.to].load;
            double flow = 0.0;

            if (fromLoad < 0 && toLoad > 0)
                flow = std::min(-fromLoad, toLoad);
            else if (fromLoad > 0 && toLoad < 0)
                flow = -std::min(fromLoad, -toLoad);

            l.flow = flow;
            l.loading = std::min(1.0, std::abs(flow) / l.capacity);

            nodes[l.from].load += flow;
            nodes[l.to].load -= flow;

            // --- Overload propagation ---
            if (std::abs(flow) > l.capacity) {
                nodes[l.to].blackout = true;
                nodes[l.to].load = 0.0;
            }
        }

        // --- 4. Voltage drops ---
        for (auto &l : lines) {
            double P = nodes[l.to].load;
            double V_drop = P * l.reactance;
            nodes[l.to].voltage -= V_drop;
            l.loading = std::min(1.0, std::abs(P) / l.capacity);
        }

        // --- 5. Blackout check ---
        for (auto &n : nodes) {
            if (n.voltage < 0.85 * V_ref) {
                n.blackout = true;
                n.load = 0.0;
            } else if (!n.blackout) {
                n.blackout = false;
            }
        }
    }

    // --- 8. JSON export ---
    json::object toJSON(int step, double t0, double currentTime) {
        std::lock_guard<std::mutex> lock(mtx);

        json::object j;
        j["step"] = step;
        j["t_0"] = t0;
        j["current_time"] = currentTime;

        json::array nodeArr;
        for (size_t i = 0; i < nodes.size(); i++) {
            nodeArr.push_back({
                                      {"id", i},
                                      {"name", nodes[i].name},
                                      {"type", nodes[i].type == NodeType::Generator ? "Generator" : nodes[i].type == NodeType::Consumer ? "Consumer" : "Battery"},
                                      {"voltage", nodes[i].voltage},
                                      {"load", nodes[i].load},
                                      {"soc", nodes[i].soc},
                                      {"capacity", nodes[i].capacity},
                                      {"blackout", nodes[i].blackout}
                              });
        }

        json::array lineArr;
        for (auto &l : lines) {
            lineArr.push_back({
                                      {"source", l.from},
                                      {"target", l.to},
                                      {"capacity", l.capacity},
                                      {"loading", l.loading},
                                      {"flow", l.flow}
                              });
        }

        j["nodes"] = nodeArr;
        j["lines"] = lineArr;

        return j;
    }
};

// --- WebSocket server ---
void websocket_server(Network& net, unsigned short port) {
    asio::io_context ioc;
    tcp::acceptor acceptor{ioc, {tcp::v4(), port}};
    std::cout << "WebSocket server listening on port " << port << "...\n";

    std::ofstream logFile("simulation_log.json");
    logFile << "[\n";

    double t_0 = 8.0; // Default start hour

    while (true) {
        tcp::socket socket{ioc};
        acceptor.accept(socket);

        std::thread([socket = std::move(socket), &net, &logFile, &t_0]() mutable {
            try {
                websocket::stream<tcp::socket> ws{std::move(socket)};
                ws.accept();

                bool running = false;
                int step = 0;

                std::thread reader([&]() {
                    try {
                        while (true) {
                            beast::flat_buffer buffer;
                            ws.read(buffer);
                            std::string cmd = beast::buffers_to_string(buffer.data());

                            if (cmd == "start") running = true;
                            if (cmd == "stop") running = false;

                            // --- Dashboard sets t_0 dynamically ---
                            if (cmd.rfind("set_t0 ", 0) == 0) {
                                try {
                                    double new_t0 = std::stod(cmd.substr(7));
                                    if (new_t0 < 0.0 || new_t0 >= 24.0)
                                        throw std::out_of_range("t_0 must be 0–24");

                                    std::lock_guard<std::mutex> lock(net.mtx);
                                    t_0 = new_t0;
                                    std::cout << "Simulation start time set to " << t_0 << " hours.\n";

                                } catch (const std::exception& e) {
                                    std::cout << "Error setting t_0: " << e.what() << "\n";
                                }
                            }
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "WebSocket reader error: " << e.what() << "\n";
                    }
                });

                std::default_random_engine generator;
                std::normal_distribution<double> noise(0.0, 2.0);

                while (true) {
                    if (running) {
                        double currentHours = t_0 + step * HOURS_PER_STEP;

                        for (auto &n : net.nodes) {
                            if (n.name == "Solar Farm")
                                n.load = -50.0 * (0.5 + 0.5 * sin((currentHours / 24.0) * 2 * M_PI)) + noise(generator);
                            if (n.name == "Wind Farm")
                                n.load = -30.0 * (0.7 + 0.3 * sin((currentHours / 24.0) * 2 * M_PI * 3)) + noise(generator);
                            if (n.name == "City Load")
                                n.load = 50.0 * (0.8 + 0.2 * sin((currentHours / 24.0) * 2 * M_PI));
                            if (n.name == "Industry Load")
                                n.load = 80.0 * (0.9 + 0.1 * sin((currentHours / 24.0) * 2 * M_PI * 0.5));
                            if (n.name == "Residential")
                                n.load = 40.0 * (0.7 + 0.3 * sin((currentHours / 24.0) * 2 * M_PI * 0.7));
                        }

                        net.simulateStep();
                        auto msg = json::serialize(net.toJSON(step, t_0, fmod(currentHours, 24.0)));

                        {
                            std::lock_guard<std::mutex> lock(net.mtx);
                            ws.write(asio::buffer(msg));
                        }

                        {
                            logFile << msg << ",\n";
                        }

                        std::cout << "Step " << step << std::endl;
                        step++;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }

                reader.join();
            } catch (const std::exception &e) {
                std::cerr << "WebSocket client error: " << e.what() << "\n";
            }
        }).detach();
    }

    logFile << "{}\n]";
    logFile.close();
}

int main() {
    Network net;

    // --- Load network config from JSON ---
    net.loadFromJSON("config.json");

    // Fallback default network if not loaded
    if (net.nodes.empty() || net.lines.empty()) {
        net.loadFromJSON("testcases/default.json");
    }

    std::thread server_thread([&net]() { websocket_server(net, 9002); });
    server_thread.join();
    return 0;
}