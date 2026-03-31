import React, { useEffect, useState, useRef } from "react";
import { LineChart, Line, XAxis, YAxis, Tooltip, CartesianGrid, Legend } from "recharts";
import ForceGraph2D from "react-force-graph-2d";

function App() {
    const [currentStep, setCurrentStep] = useState(0);
    const [chartData, setChartData] = useState([]);
    const [graphData, setGraphData] = useState({ nodes: [], links: [] });
    const [running, setRunning] = useState(false);
    const [wsConnected, setWsConnected] = useState(false);
    const [history, setHistory] = useState([]);
    const [startHour, setStartHour] = useState(8);
    const [currentT0, setCurrentT0] = useState(8);
    const [simTime, setSimTime] = useState(0);

    const ws = useRef(null);

    const V_REF = 230.0;
    const S_BASE = 100.0;

    const getVoltagePU = (v) => v / V_REF;
    const getLoadPU = (p) => p / S_BASE;

    const getVoltageColor = (vpu) => {
        if (vpu < 0.9) return "red";
        if (vpu < 0.95) return "orange";
        return "green";
    };

    const formatSimTime = (hours) => {
        const h = Math.floor(hours);
        const m = Math.floor((hours - h) * 60);
        return `${h.toString().padStart(2,'0')}:${m.toString().padStart(2,'0')}`;
    };

    useEffect(() => {
        ws.current = new WebSocket("ws://localhost:9002");

        ws.current.onopen = () => setWsConnected(true);
        ws.current.onclose = () => setWsConnected(false);

        ws.current.onmessage = (event) => {
            try {
                const json = JSON.parse(event.data);

                if (json.t_0 !== undefined) setCurrentT0(json.t_0);
                if (json.current_time !== undefined) setSimTime(json.current_time);

                const newChartData = json.nodes.map(n => ({
                    name: n.name,
                    voltage: getVoltagePU(n.voltage),
                    load: getLoadPU(n.load),
                    soc: n.soc
                }));
                setChartData(newChartData);
                setCurrentStep(json.step);

                const nodes = json.nodes.map(n => ({
                    id: n.id,
                    name: n.name,
                    voltage: getVoltagePU(n.voltage),
                    soc: n.soc
                }));
                const links = json.lines.map(l => ({
                    source: l.source,
                    target: l.target,
                    loading: l.loading
                }));

                setGraphData(prev => {
                    if (prev.nodes.length === 0) return { nodes, links };
                    prev.nodes.forEach((node, i) => {
                        node.voltage = getVoltagePU(json.nodes[i].voltage);
                        node.soc = json.nodes[i].soc;
                    });
                    prev.links.forEach((link, i) => {
                        link.loading = json.lines[i].loading;
                    });
                    return prev;
                });

                const totalLoadKW = json.nodes.reduce((s, n) => s + n.load, 0);
                setHistory(prev => [...prev.slice(-20), { step: json.step, load: totalLoadKW }]);

            } catch (err) {
                console.error("Invalid WS message", err);
            }
        };
    }, []);

    const toggleSimulation = () => {
        if (!ws.current || ws.current.readyState !== WebSocket.OPEN) return;
        if (!running) {
            ws.current.send("start");
            setRunning(true);
        } else {
            ws.current.send("stop");
            setRunning(false);
        }
    };

    const updateStartHour = () => {
        if (ws.current && ws.current.readyState === WebSocket.OPEN) {
            ws.current.send(`set_t0 ${startHour}`);
            setCurrentT0(startHour);
        }
    };

    const generation = chartData.filter(n => n.load < 0).reduce((s, n) => s + n.load, 0) * S_BASE;
    const consumption = chartData.filter(n => n.load > 0).reduce((s, n) => s + n.load, 0) * S_BASE;
    const imbalance = generation + consumption;
    const hasProblem = chartData.some(n => n.voltage < 0.9);
    const battery = chartData.find(n => n.name === "Battery");

    return (
        <div style={{ padding: "20px", background: "#f4f6f8", minHeight: "100vh", fontFamily: "Arial, sans-serif" }}>
            <h1>MiniPowerGrid Dashboard</h1>

            {/* Simulation Info + Controls */}
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "15px", flexWrap: "wrap" }}>
                <div style={{ marginBottom: "10px" }}>
                    <span style={{ marginRight: "20px" }}><b>Step:</b> {currentStep}</span>
                    <span style={{ marginRight: "20px" }}><b>Simulation Time:</b> {formatSimTime(simTime)}</span>
                    <span><b>Start Time:</b> {formatSimTime(currentT0)}</span>
                </div>
                <div>
                    <button onClick={toggleSimulation} disabled={!wsConnected} style={{ marginRight: "10px" }}>
                        {running ? "Stop" : "Start"}
                    </button>
                    <input
                        type="number"
                        min="0"
                        max="23.99"
                        step="0.01"
                        value={startHour}
                        onChange={(e) => setStartHour(parseFloat(e.target.value))}
                        style={{ width: "60px", marginRight: "5px" }}
                    />
                    <button onClick={updateStartHour} disabled={!wsConnected}>Set Start Time</button>
                </div>
            </div>

            <div style={{
                display: "grid",
                gridTemplateColumns: "300px 1fr",
                gridTemplateRows: "300px 420px 220px",
                gap: "20px",
                marginTop: "20px"
            }}>

                {/* System Status & Fehleranzeige */}
                <div style={{ border: "1px solid #ccc", padding: "15px", borderRadius: "10px", display: "flex", flexDirection: "column", justifyContent: "space-between" }}>
                    <h3>System Status</h3>
                    <div>
                        <p>Generation: {generation.toFixed(2)} kW</p>
                        <p>Consumption: {consumption.toFixed(2)} kW</p>
                        <p>Balance: {imbalance.toFixed(2)} kW</p>
                        {battery && <p>Battery SOC: {(battery.soc * 100).toFixed(2)}%</p>}
                    </div>
                    {hasProblem && (
                        <div style={{ marginTop: "10px", padding: "10px", backgroundColor: "#ffe6e6", borderRadius: "5px", color: "red", fontWeight: "bold" }}>
                            ⚠ Voltage instability detected!
                        </div>
                    )}
                </div>

                {/* Voltage Chart */}
                <div style={{ border: "1px solid #ccc", padding: "10px", borderRadius: "10px" }}>
                    <LineChart width={600} height={280} data={chartData}>
                        <CartesianGrid stroke="#ccc" />
                        <XAxis dataKey="name" />
                        <YAxis domain={[0, 1.2]} />
                        <Tooltip formatter={(value) => value.toFixed(2)} />
                        <Legend />
                        <Line type="monotone" dataKey="voltage" name="Voltage (p.u.)" stroke="#82ca9d" />
                        <Line type="monotone" dataKey="load" name="Load (p.u.)" stroke="#8884d8" />
                    </LineChart>
                </div>

                {/* Network Graph */}
                <div style={{ gridColumn: "1 / span 2", border: "1px solid #ccc", borderRadius: "10px", padding: "10px" }}>
                    <div style={{ marginBottom: "5px" }}>
                        <b>Voltage Status:</b>
                        <span style={{ color: "green", marginLeft: "10px" }}>Normal</span>
                        <span style={{ color: "orange", marginLeft: "10px" }}>Low</span>
                        <span style={{ color: "red", marginLeft: "10px" }}>Critical</span>
                    </div>
                    <div style={{ marginBottom: "10px" }}>
                        <b>Line Loading:</b>
                        <span style={{ color: "gray", marginLeft: "10px" }}>Normal</span>
                        <span style={{ color: "red", marginLeft: "10px" }}>Overload</span>
                    </div>
                    <ForceGraph2D
                        width={900}
                        height={380}
                        graphData={graphData}
                        cooldownTicks={100}
                        d3VelocityDecay={0.3}
                        d3AlphaDecay={0.02}
                        nodeCanvasObject={(node, ctx) => {
                            const color = getVoltageColor(node.voltage);
                            ctx.fillStyle = color;
                            ctx.beginPath();
                            ctx.arc(node.x, node.y, 7, 0, 2 * Math.PI);
                            ctx.fill();
                            ctx.fillStyle = "black";
                            ctx.fillText(node.name, node.x + 10, node.y + 4);
                        }}
                        linkWidth={l => 1 + l.loading * 6}
                        linkColor={l => l.loading > 0.9 ? "red" : "gray"}
                    />
                </div>

                {/* Load History */}
                <div style={{ gridColumn: "1 / span 2", border: "1px solid #ccc", borderRadius: "10px", padding: "10px" }}>
                    <h3>Load History (kW)</h3>
                    <LineChart width={900} height={150} data={history}>
                        <CartesianGrid stroke="#ccc" />
                        <XAxis dataKey="step" />
                        <YAxis />
                        <Tooltip formatter={(value) => value.toFixed(2)} />
                        <Line type="monotone" dataKey="load" stroke="#8884d8" />
                    </LineChart>
                </div>

            </div>
        </div>
    );
}

export default App;