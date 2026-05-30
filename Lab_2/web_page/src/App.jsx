import React, { useCallback, useEffect, useRef, useState } from "react";
import { usePs4Controller } from "./ps4Controller";

const DEFAULT_API = import.meta.env.VITE_API_URL || "http://192.168.1.98:8000";
const RECONNECT_DELAY_MS = 500;
const DEFAULT_SPEED = 200;
const SPEED_STORAGE_KEY = "esp_robot_speed";

const DIRECTION_COMMANDS = {
  forward: "left",
  right: "forward",
  back: "right",
  left: "back",
};

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

function normalizeSpeed(value) {
  const speed = Number(value);
  if (!Number.isFinite(speed)) return DEFAULT_SPEED;
  return Math.min(255, Math.max(50, Math.round(speed)));
}

function loadStoredSpeed() {
  return normalizeSpeed(localStorage.getItem(SPEED_STORAGE_KEY));
}

export default function App() {
  const [status, setStatus] = useState("Conectando...");
  const [apiBase, setApiBase] = useState(
    () => localStorage.getItem("esp_api_url") || DEFAULT_API
  );
  const [ws, setWs] = useState(null);
  const [speed, setSpeed] = useState(loadStoredSpeed);
  const [isConnected, setIsConnected] = useState(false);
  const [currentDirection, setCurrentDirection] = useState(null);
  const currentDirectionRef = useRef(null);
  const speedRef = useRef(speed);
  const reconnectTimerRef = useRef(null);
  const manualCloseRef = useRef(false);

  const saveSpeed = useCallback((nextSpeed) => {
    const normalizedSpeed = normalizeSpeed(nextSpeed);
    speedRef.current = normalizedSpeed;
    localStorage.setItem(SPEED_STORAGE_KEY, String(normalizedSpeed));
    setSpeed(normalizedSpeed);
  }, []);

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
      newWs.send(JSON.stringify({ action: "set_speed", speed: speedRef.current }));
      setIsConnected(true);
      setStatus(`Conectado: ${wsUrl} | velocidad ${speedRef.current}/255`);
    };

    newWs.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        console.log("Respuesta del servidor:", data);
        if (data.speed !== undefined) {
          saveSpeed(data.speed);
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
      setStatus(`Desconectado (code ${event.code}). Reintentando rapido...`);
      reconnectTimerRef.current = setTimeout(
        () => connectWebSocket(targetApi),
        RECONNECT_DELAY_MS
      );
    };

    setWs(newWs);
  }, [apiBase, saveSpeed]);

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
    currentDirectionRef.current = direction;
    sendCommand({ action: "move", direction });
    setStatus(`Moviendo: ${direction}`);
  }, [sendCommand]);

  /* Stop handler - release */
  const handleMoveStop = useCallback(() => {
    setCurrentDirection(null);
    currentDirectionRef.current = null;
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

  const { controllerStatus } = usePs4Controller({
    directionCommands: DIRECTION_COMMANDS,
    onMoveStart: handleMoveStart,
    onMoveStop: handleMoveStop,
    onSpeedDown: handleSpeedDown,
    onSpeedUp: handleSpeedUp,
  });

  /* Keyboard controls */
  useEffect(() => {
    const map = {
      ArrowUp: DIRECTION_COMMANDS.forward,
      ArrowDown: DIRECTION_COMMANDS.back,
      ArrowLeft: DIRECTION_COMMANDS.left,
      ArrowRight: DIRECTION_COMMANDS.right,
      KeyW: DIRECTION_COMMANDS.forward,
      KeyS: DIRECTION_COMMANDS.back,
      KeyA: DIRECTION_COMMANDS.left,
      KeyD: DIRECTION_COMMANDS.right,
    };
    const speedMap = {
      KeyQ: handleSpeedUp,
      KeyE: handleSpeedDown,
    };

    const activeDirections = new Set();
    const activeSpeedKeys = new Set();
    let isSaveKeyActive = false;

    function resolveDirection() {
      const hasForward = activeDirections.has("forward");
      const hasBack = activeDirections.has("back");
      const hasLeft = activeDirections.has("left");
      const hasRight = activeDirections.has("right");

      if (hasForward && hasBack) return null;
      if (hasLeft && hasRight) return null;

      if (hasForward && hasRight) return "forward_right";
      if (hasForward && hasLeft) return "forward_left";
      if (hasBack && hasRight) return "back_right";
      if (hasBack && hasLeft) return "back_left";

      if (hasForward) return "forward";
      if (hasBack) return "back";
      if (hasLeft) return "left";
      if (hasRight) return "right";

      return null;
    }

    function applyDirection(nextDirection) {
      if (nextDirection === currentDirectionRef.current) return;
      if (nextDirection) {
        handleMoveStart(nextDirection);
      } else {
        handleMoveStop();
      }
    }

    function isTypingTarget(target) {
      return (
        target instanceof HTMLElement &&
        (target.isContentEditable ||
          target.tagName === "INPUT" ||
          target.tagName === "TEXTAREA" ||
          target.tagName === "SELECT")
      );
    }

    function onKeyDown(e) {
      if (e.code === "Space" && !isSaveKeyActive) {
        isSaveKeyActive = true;
        e.preventDefault();
        saveApiBase();
        return;
      }

      if (isTypingTarget(e.target)) return;
      const speedHandler = speedMap[e.code];
      if (speedHandler && !activeSpeedKeys.has(e.code)) {
        activeSpeedKeys.add(e.code);
        e.preventDefault();
        speedHandler();
        return;
      }

      const direction = map[e.code] || map[e.key];
      if (direction && !activeDirections.has(direction)) {
        activeDirections.add(direction);
        e.preventDefault();
        applyDirection(resolveDirection());
      }
    }

    function onKeyUp(e) {
      if (e.code === "Space" && isSaveKeyActive) {
        isSaveKeyActive = false;
        e.preventDefault();
        return;
      }

      if (isTypingTarget(e.target)) return;
      if (speedMap[e.code] && activeSpeedKeys.has(e.code)) {
        activeSpeedKeys.delete(e.code);
        e.preventDefault();
        return;
      }

      const direction = map[e.code] || map[e.key];
      if (direction && activeDirections.has(direction)) {
        activeDirections.delete(direction);
        e.preventDefault();
        applyDirection(resolveDirection());
      }
    }

    window.addEventListener("keydown", onKeyDown);
    window.addEventListener("keyup", onKeyUp);

    return () => {
      window.removeEventListener("keydown", onKeyDown);
      window.removeEventListener("keyup", onKeyUp);
    };
  }, [handleMoveStart, handleMoveStop, handleSpeedDown, handleSpeedUp, saveApiBase]);

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
            className={`control-btn ${currentDirection === DIRECTION_COMMANDS.forward ? "active" : ""}`}
            onMouseDown={() => handleMoveStart(DIRECTION_COMMANDS.forward)}
            onMouseUp={handleMoveStop}
            onMouseLeave={handleMoveStop}
          >
            ▲
          </button>
        </div>
        <div className="row">
          <button
            className={`control-btn ${currentDirection === DIRECTION_COMMANDS.left ? "active" : ""}`}
            onMouseDown={() => handleMoveStart(DIRECTION_COMMANDS.left)}
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
            className={`control-btn ${currentDirection === DIRECTION_COMMANDS.right ? "active" : ""}`}
            onMouseDown={() => handleMoveStart(DIRECTION_COMMANDS.right)}
            onMouseUp={handleMoveStop}
            onMouseLeave={handleMoveStop}
          >
            ▶
          </button>
        </div>
        <div className="row">
          <button
            className={`control-btn ${currentDirection === DIRECTION_COMMANDS.back ? "active" : ""}`}
            onMouseDown={() => handleMoveStart(DIRECTION_COMMANDS.back)}
            onMouseUp={handleMoveStop}
            onMouseLeave={handleMoveStop}
          >
            ▼
          </button>
        </div>
      </div>

      <p className="hint">
        Modo WebSocket: usa botones, flechas/WASD, Q/E, espacio o control PS4
        {` | ${controllerStatus}`}
      </p>
    </div>
  );
}
