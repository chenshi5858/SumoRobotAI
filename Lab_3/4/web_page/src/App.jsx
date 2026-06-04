import React, { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { usePs4Controller } from "./ps4Controller";

const DEFAULT_API = import.meta.env.VITE_API_URL || "http://192.168.1.98:8000";
const RECONNECT_DELAY_MS = 500;
const DEFAULT_SPEED = 175;
const MIN_SPEED = 80;
const MAX_SPEED = 190;
const SPEED_STORAGE_KEY = "lab3_pose_speed";
const MAP_SCALE = 120;
const TRAIL_LIMIT = 240;

const DIRECTION_COMMANDS = {
  forward: "forward",
  right: "right",
  back: "back",
  left: "left",
};

const EMPTY_POSE = {
  x: 0,
  y: 0,
  theta: 0,
  theta_deg: 0,
  v: 0,
  omega: 0,
  pwm_left: 0,
  pwm_right: 0,
  ts_ms: 0,
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
  return Math.min(MAX_SPEED, Math.max(MIN_SPEED, Math.round(speed)));
}

function loadStoredSpeed() {
  return normalizeSpeed(localStorage.getItem(SPEED_STORAGE_KEY));
}

function formatNumber(value, digits = 2) {
  if (!Number.isFinite(value)) return "--";
  return value.toFixed(digits);
}

export default function App() {
  const [status, setStatus] = useState("Connecting...");
  const [apiBase, setApiBase] = useState(
    () => localStorage.getItem("esp_api_url") || DEFAULT_API
  );
  const [ws, setWs] = useState(null);
  const [speed, setSpeed] = useState(loadStoredSpeed);
  const [isConnected, setIsConnected] = useState(false);
  const [currentDirection, setCurrentDirection] = useState(null);
  const [pose, setPose] = useState(EMPTY_POSE);
  const [trail, setTrail] = useState([]);

  const currentDirectionRef = useRef(null);
  const speedRef = useRef(speed);
  const reconnectTimerRef = useRef(null);
  const manualCloseRef = useRef(false);
  const trailRef = useRef([]);

  const saveSpeed = useCallback((nextSpeed) => {
    const normalizedSpeed = normalizeSpeed(nextSpeed);
    speedRef.current = normalizedSpeed;
    localStorage.setItem(SPEED_STORAGE_KEY, String(normalizedSpeed));
    setSpeed(normalizedSpeed);
  }, []);

  const updateTrail = useCallback((nextPose) => {
    if (!Number.isFinite(nextPose.x) || !Number.isFinite(nextPose.y)) {
      return;
    }

    const nextPoint = {
      x: nextPose.x * MAP_SCALE,
      y: -nextPose.y * MAP_SCALE,
    };

    const prevPoint = trailRef.current[trailRef.current.length - 1];
    if (prevPoint) {
      const dx = nextPoint.x - prevPoint.x;
      const dy = nextPoint.y - prevPoint.y;
      if (Math.hypot(dx, dy) < 2) {
        return;
      }
    }

    const nextTrail = [...trailRef.current, nextPoint];
    if (nextTrail.length > TRAIL_LIMIT) {
      nextTrail.splice(0, nextTrail.length - TRAIL_LIMIT);
    }

    trailRef.current = nextTrail;
    setTrail(nextTrail);
  }, []);

  const connectWebSocket = useCallback((nextApiBase) => {
    const targetApi = normalizeApiBase(nextApiBase ?? apiBase);
    const wsUrl = buildWsUrl(targetApi);

    if (!wsUrl) {
      setStatus("Invalid URL. Use IP:PORT or http://IP:PORT");
      setIsConnected(false);
      return;
    }

    if (reconnectTimerRef.current) {
      clearTimeout(reconnectTimerRef.current);
      reconnectTimerRef.current = null;
    }

    manualCloseRef.current = false;
    const newWs = new WebSocket(wsUrl);

    newWs.onopen = () => {
      newWs.send(JSON.stringify({ action: "set_speed", speed: speedRef.current }));
      setIsConnected(true);
      setStatus(`Connected: ${wsUrl} | speed ${speedRef.current}/${MAX_SPEED}`);
    };

    newWs.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        if (data.speed !== undefined) {
          saveSpeed(data.speed);
        }
        if (data.type === "pose" || (data.x !== undefined && data.y !== undefined)) {
          const nextPose = {
            ...EMPTY_POSE,
            ...data,
          };
          setPose(nextPose);
          updateTrail(nextPose);
        }
      } catch (error) {
        console.error("Error parsing message", error);
      }
    };

    newWs.onerror = (error) => {
      console.error("WebSocket error:", error);
      setStatus(`WebSocket error at ${wsUrl}`);
      setIsConnected(false);
    };

    newWs.onclose = (event) => {
      setIsConnected(false);
      if (manualCloseRef.current) {
        return;
      }
      setStatus(`Disconnected (code ${event.code}). Reconnecting...`);
      reconnectTimerRef.current = setTimeout(
        () => connectWebSocket(targetApi),
        RECONNECT_DELAY_MS
      );
    };

    setWs(newWs);
  }, [apiBase, saveSpeed, updateTrail]);

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

  const saveApiBase = useCallback(() => {
    const normalized = normalizeApiBase(apiBase);
    localStorage.setItem("esp_api_url", normalized);
    setApiBase(normalized);
    setStatus(`Endpoint saved: ${normalized}`);

    if (ws) {
      manualCloseRef.current = true;
      ws.close();
    }
    setTimeout(() => connectWebSocket(normalized), 300);
  }, [apiBase, ws, connectWebSocket]);

  const sendCommand = useCallback((cmd) => {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      return;
    }
    ws.send(JSON.stringify(cmd));
  }, [ws]);

  const handleMoveStart = useCallback((direction) => {
    setCurrentDirection(direction);
    currentDirectionRef.current = direction;
    sendCommand({ action: "move", direction });
    setStatus(`Moving: ${direction}`);
  }, [sendCommand]);

  const handleMoveStop = useCallback(() => {
    setCurrentDirection(null);
    currentDirectionRef.current = null;
    sendCommand({ action: "stop" });
    setStatus("Stopped");
  }, [sendCommand]);

  const handleSpeedUp = useCallback(() => {
    sendCommand({ action: "speed_up" });
  }, [sendCommand]);

  const handleSpeedDown = useCallback(() => {
    sendCommand({ action: "speed_down" });
  }, [sendCommand]);

  const handleResetPose = useCallback(() => {
    sendCommand({ action: "reset_pose" });
    setTrail([]);
    trailRef.current = [];
  }, [sendCommand]);

  const { controllerStatus } = usePs4Controller({
    directionCommands: DIRECTION_COMMANDS,
    onMoveStart: handleMoveStart,
    onMoveStop: handleMoveStop,
    onSpeedDown: handleSpeedDown,
    onSpeedUp: handleSpeedUp,
  });

  useEffect(() => {
    const map = {
      ArrowUp: "forward",
      ArrowDown: "back",
      ArrowLeft: "left",
      ArrowRight: "right",
      KeyW: "forward",
      KeyS: "back",
      KeyA: "left",
      KeyD: "right",
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

  const headingDeg = Number.isFinite(pose.theta)
    ? (pose.theta * 180) / Math.PI
    : 0;
  const robotX = pose.x * MAP_SCALE;
  const robotY = -pose.y * MAP_SCALE;

  const trailPath = useMemo(() => {
    if (!trail.length) return "";
    return trail
      .map((point, index) => `${index === 0 ? "M" : "L"} ${point.x} ${point.y}`)
      .join(" ");
  }, [trail]);

  return (
    <div className="app">
      <header className="hero">
        <div>
          <p className="eyebrow">Lab 3 / Ej4</p>
          <h1>Robot Pose Stream</h1>
          <p className="subtitle">WebSocket control and live pose tracking</p>
        </div>
        <div className={`status ${isConnected ? "connected" : "disconnected"}`}>
          {status}
        </div>
      </header>

      <section className="panel">
        <div className="panel-title">Connection</div>
        <div className="endpoint-row">
          <input
            className="endpoint-input"
            value={apiBase}
            onChange={(e) => setApiBase(e.target.value)}
            placeholder="http://192.168.1.98:8000"
          />
          <button className="btn" onClick={saveApiBase}>
            Save IP
          </button>
        </div>
      </section>

      <section className="grid">
        <div className="panel">
          <div className="panel-title">Control</div>
          <div className="speed-section">
            <div className="speed-row">
              <span>Speed</span>
              <strong>{speed}/{MAX_SPEED}</strong>
            </div>
            <div className="speed-track">
              <div className="speed-fill" style={{ width: `${(speed / MAX_SPEED) * 100}%` }} />
            </div>
            <div className="speed-controls">
              <button className="btn ghost" onClick={handleSpeedDown}>
                Slower (Q)
              </button>
              <button className="btn ghost" onClick={handleSpeedUp}>
                Faster (E)
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
                ^
              </button>
            </div>
            <div className="row">
              <button
                className={`control-btn ${currentDirection === "left" ? "active" : ""}`}
                onMouseDown={() => handleMoveStart("left")}
                onMouseUp={handleMoveStop}
                onMouseLeave={handleMoveStop}
              >
                <span>{"<"}</span>
              </button>
              <button className="control-btn stop-btn" onMouseDown={handleMoveStop}>
                STOP
              </button>
              <button
                className={`control-btn ${currentDirection === "right" ? "active" : ""}`}
                onMouseDown={() => handleMoveStart("right")}
                onMouseUp={handleMoveStop}
                onMouseLeave={handleMoveStop}
              >
                <span>{">"}</span>
              </button>
            </div>
            <div className="row">
              <button
                className={`control-btn ${currentDirection === "back" ? "active" : ""}`}
                onMouseDown={() => handleMoveStart("back")}
                onMouseUp={handleMoveStop}
                onMouseLeave={handleMoveStop}
              >
                v
              </button>
            </div>
          </div>

          <div className="helper">
            <span>WASD or arrows, Q/E speed, Space save IP</span>
            <span>{controllerStatus}</span>
          </div>
        </div>

        <div className="panel">
          <div className="panel-title">Pose</div>
          <div className="map">
            <svg viewBox="-220 -220 440 440" className="map-svg" aria-hidden="true">
              <defs>
                <pattern id="grid" width="40" height="40" patternUnits="userSpaceOnUse">
                  <path d="M 40 0 L 0 0 0 40" fill="none" stroke="rgba(255,255,255,0.05)" strokeWidth="1" />
                </pattern>
              </defs>
              <rect x="-220" y="-220" width="440" height="440" fill="url(#grid)" />
              {trailPath && (
                <path d={trailPath} fill="none" stroke="rgba(57, 211, 198, 0.55)" strokeWidth="2" />
              )}
              <g transform={`translate(${robotX} ${robotY}) rotate(${headingDeg})`}>
                <polygon points="0,-12 8,10 -8,10" fill="#39d3c6" />
                <circle cx="0" cy="0" r="3" fill="#0b1020" />
              </g>
            </svg>
            <div className="map-readout">
              <div>
                <span>X</span>
                <strong>{formatNumber(pose.x, 3)} m</strong>
              </div>
              <div>
                <span>Y</span>
                <strong>{formatNumber(pose.y, 3)} m</strong>
              </div>
              <div>
                <span>Theta</span>
                <strong>{formatNumber(pose.theta_deg, 1)} deg</strong>
              </div>
            </div>
          </div>

          <div className="telemetry">
            <div>
              <span>V</span>
              <strong>{formatNumber(pose.v, 3)} m/s</strong>
            </div>
            <div>
              <span>Omega</span>
              <strong>{formatNumber(pose.omega, 3)} rad/s</strong>
            </div>
            <div>
              <span>PWM L</span>
              <strong>{pose.pwm_left}</strong>
            </div>
            <div>
              <span>PWM R</span>
              <strong>{pose.pwm_right}</strong>
            </div>
          </div>

          <button className="btn warning" onClick={handleResetPose}>
            Reset pose (0,0,0)
          </button>
        </div>
      </section>
    </div>
  );
}
