import React, { useCallback, useEffect, useRef, useState } from "react";

const DEFAULT_API = import.meta.env.VITE_API_URL || "http://192.168.1.98:8000";

function normalizeApiBase(value) {
  const raw = (value || "").trim();
  if (!raw) return DEFAULT_API;
  if (/^https?:\/\//i.test(raw)) return raw.replace(/\/$/, "");
  return `http://${raw.replace(/\/$/, "")}`;
}

function buildWsUrl(apiBase) {
  try {
    const u = new URL(normalizeApiBase(apiBase));
    const wsProtocol = u.protocol === "https:" ? "wss:" : "ws:";
    return `${wsProtocol}//${u.host}/ws`;
  } catch {
    return null;
  }
}

export default function App() {
  const [status, setStatus] = useState("Conectando...");
  const [apiBase, setApiBase] = useState(
    () => localStorage.getItem("esp_api_url") || DEFAULT_API
  );
  const [ws, setWs] = useState(null);
  const [speed, setSpeed] = useState(200);
  const [isConnected, setIsConnected] = useState(false);
  const [currentDirection, setCurrentDirection] = useState(null);
  const reconnectTimerRef = useRef(null);
  const manualCloseRef = useRef(false);

  /* Connect to WebSocket */
  const connectWebSocket = useCallback((nextApiBase) => {
    const targetApi = normalizeApiBase(nextApiBase ?? apiBase);
    const wsUrl = buildWsUrl(targetApi);

    if (!wsUrl) {
      setStatus("URL invalida. Usa formato IP:PUERTO o http://IP:PUERTO");
      setIsConnected(false);
      return;
    }

    console.log("Conectando a", wsUrl);

    if (reconnectTimerRef.current) {
      clearTimeout(reconnectTimerRef.current);
      reconnectTimerRef.current = null;
    }

    manualCloseRef.current = false;
    const newWs = new WebSocket(wsUrl);

    newWs.onopen = () => {
      console.log("WebSocket conectado");
      setIsConnected(true);
      setStatus(`Conectado: ${wsUrl}`);
    };

    newWs.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        console.log("Respuesta del servidor:", data);
        if (data.speed !== undefined) {
          setSpeed(data.speed);
        }
      } catch (e) {
        console.error("Error parsing message", e);
      }
    };

    newWs.onerror = (error) => {
      console.error("WebSocket error:", error);
      setStatus(`Error WebSocket en ${wsUrl}`);
      setIsConnected(false);
    };

    newWs.onclose = (event) => {
      console.log("WebSocket desconectado", event.code, event.reason);
      setIsConnected(false);
      if (manualCloseRef.current) {
        return;
      }
      setStatus(`Desconectado (code ${event.code}). Reintentando...`);
      reconnectTimerRef.current = setTimeout(() => connectWebSocket(targetApi), 3000);
    };

    setWs(newWs);
  }, [apiBase]);

  /* Initial connection */
  useEffect(() => {
    connectWebSocket();
    return () => {
      manualCloseRef.current = true;
      if (reconnectTimerRef.current) {
        clearTimeout(reconnectTimerRef.current);
      }
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.close();
      }
    };
  }, []);

  /* Save API base */
  const saveApiBase = useCallback(() => {
    const normalized = normalizeApiBase(apiBase);
    localStorage.setItem("esp_api_url", normalized);
    setApiBase(normalized);
    setStatus(`Endpoint guardado: ${normalized}`);

    if (ws) {
      manualCloseRef.current = true;
      ws.close();
    }
    setTimeout(() => connectWebSocket(normalized), 300);
  }, [apiBase, ws, connectWebSocket]);

  /* Send WebSocket command */
  const sendCommand = useCallback((cmd) => {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      console.warn("WebSocket not connected");
      return;
    }
    ws.send(JSON.stringify(cmd));
  }, [ws]);

  /* Move handler - press down */
  const handleMoveStart = useCallback((direction) => {
    setCurrentDirection(direction);
    sendCommand({ action: "move", direction });
    setStatus(`Moviendo: ${direction}`);
  }, [sendCommand]);

  /* Stop handler - release */
  const handleMoveStop = useCallback(() => {
    setCurrentDirection(null);
    sendCommand({ action: "stop" });
    setStatus("Detenido");
  }, [sendCommand]);

  /* Speed up */
  const handleSpeedUp = useCallback(() => {
    sendCommand({ action: "speed_up" });
  }, [sendCommand]);

  /* Speed down */
  const handleSpeedDown = useCallback(() => {
    sendCommand({ action: "speed_down" });
  }, [sendCommand]);

  /* Keyboard controls */
  useEffect(() => {
    const map = {
      ArrowUp: "forward",
      ArrowDown: "back",
      ArrowLeft: "left",
      ArrowRight: "right",
      w: "forward",
      s: "back",
      a: "left",
      d: "right",
    };

    const activeKeys = new Set();

    function onKeyDown(e) {
      const direction = map[e.key];
      if (direction && !activeKeys.has(e.key)) {
        activeKeys.add(e.key);
        e.preventDefault();
        handleMoveStart(direction);
      }
    }

    function onKeyUp(e) {
      const direction = map[e.key];
      if (direction && activeKeys.has(e.key)) {
        activeKeys.delete(e.key);
        e.preventDefault();
        if (activeKeys.size === 0) {
          handleMoveStop();
        }
      }
    }

    window.addEventListener("keydown", onKeyDown);
    window.addEventListener("keyup", onKeyUp);

    return () => {
      window.removeEventListener("keydown", onKeyDown);
      window.removeEventListener("keyup", onKeyUp);
    };
  }, [handleMoveStart, handleMoveStop]);

  return (
    <div className="app">
      <h1>Control del Robot por WebSocket</h1>
      
      <div className="endpoint-row">
        <input
          className="endpoint-input"
          value={apiBase}
          onChange={(e) => setApiBase(e.target.value)}
          placeholder="http://192.168.1.98:8000"
        />
        <button className="control-btn" onClick={saveApiBase}>
          Guardar IP
        </button>
      </div>

      <div className={`status ${isConnected ? "connected" : "disconnected"}`}>
        {status}
      </div>

      <div className="speed-section">
        <p className="speed-label">Velocidad: {speed}/255</p>
        <div className="speed-bar" style={{ width: `${(speed / 255) * 100}%` }}></div>
        <div className="speed-controls">
          <button className="speed-btn" onClick={handleSpeedDown}>
            ➖ Lento
          </button>
          <button className="speed-btn" onClick={handleSpeedUp}>
            Rápido ➕
          </button>
        </div>
      </div>

      <div className="pad">
        <div className="row">
          <button
            className={`control-btn ${currentDirection === "forward" ? "active" : ""}`}
            onMouseDown={() => handleMoveStart("forward")}
            onMouseUp={handleMoveStop}
            onMouseLeave={handleMoveStop}
          >
            ▲
          </button>
        </div>
        <div className="row">
          <button
            className={`control-btn ${currentDirection === "left" ? "active" : ""}`}
            onMouseDown={() => handleMoveStart("left")}
            onMouseUp={handleMoveStop}
            onMouseLeave={handleMoveStop}
          >
            ◀
          </button>
          <button
            className="control-btn stop-btn"
            onMouseDown={handleMoveStop}
          >
            ⏹
          </button>
          <button
            className={`control-btn ${currentDirection === "right" ? "active" : ""}`}
            onMouseDown={() => handleMoveStart("right")}
            onMouseUp={handleMoveStop}
            onMouseLeave={handleMoveStop}
          >
            ▶
          </button>
        </div>
        <div className="row">
          <button
            className={`control-btn ${currentDirection === "back" ? "active" : ""}`}
            onMouseDown={() => handleMoveStart("back")}
            onMouseUp={handleMoveStop}
            onMouseLeave={handleMoveStop}
          >
            ▼
          </button>
        </div>
      </div>

      <p className="hint">
        Modo WebSocket: usa botones (presiona y mantén) o flechas/WASD
      </p>
    </div>
  );
}
