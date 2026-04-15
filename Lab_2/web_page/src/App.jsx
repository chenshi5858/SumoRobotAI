import React, { useCallback, useEffect, useState, useRef } from "react";

const API_BASE = import.meta.env.VITE_API_URL || "http://localhost:8000";

async function sendCommandHTTP(direction) {
  const res = await fetch(`${API_BASE}/move`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ direction }),
  });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
}

function ControlButton({ onClick, children }) {
  return (
    <button className="control-btn" onClick={onClick}>
      {children}
    </button>
  );
}

export default function App() {
  const [status, setStatus] = useState("Listo");
  const [btConnected, setBtConnected] = useState(false);
  const charRef = useRef(null);

  const sendCommand = useCallback(async (direction) => {
    setStatus(`Enviando: ${direction}`);
    try {
      if (charRef.current) {
        // enviar por BLE (Web Bluetooth)
        const encoder = new TextEncoder();
        await charRef.current.writeValue(encoder.encode(direction));
        setStatus(`Comando BLE enviado: ${direction}`);
      } else {
        // fallback HTTP
        await sendCommandHTTP(direction);
        setStatus(`Comando HTTP enviado: ${direction}`);
      }
    } catch (err) {
      setStatus(`Error: ${err.message}`);
    }
  }, []);

  async function connectBluetooth() {
    if (!navigator.bluetooth) {
      setStatus('Web Bluetooth no está disponible en este navegador');
      return;
    }

    try {
      setStatus('Buscando dispositivo...');
      const device = await navigator.bluetooth.requestDevice({
        filters: [{ namePrefix: 'Robot' }],
        optionalServices: [0xFFE0],
      });

      device.addEventListener('gattserverdisconnected', () => {
        setBtConnected(false);
        charRef.current = null;
        setStatus('Bluetooth desconectado');
      });

      const server = await device.gatt.connect();
      const service = await server.getPrimaryService(0xFFE0);
      const characteristic = await service.getCharacteristic(0xFFE1);
      charRef.current = characteristic;
      setBtConnected(true);
      setStatus('Bluetooth conectado');
    } catch (err) {
      setStatus(`BT error: ${err.message}`);
    }
  }

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
    function onKey(e) {
      const k = map[e.key];
      if (k) {
        e.preventDefault();
        sendCommand(k);
      }
    }
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [sendCommand]);

  return (
    <div className="app">
      <h1>Control del Auto</h1>
      <div style={{ display: 'flex', gap: 8, justifyContent: 'center', marginBottom: 12 }}>
        <button className="control-btn" onClick={connectBluetooth}>
          {btConnected ? 'Conectado (BLE)' : 'Conectar Bluetooth'}
        </button>
      </div>
      <div className="pad">
        <div className="row">
          <ControlButton onClick={() => sendCommand("forward")}>▲</ControlButton>
        </div>
        <div className="row">
          <ControlButton onClick={() => sendCommand("left")}>◀</ControlButton>
          <ControlButton onClick={() => setStatus("Pausa")}>⏸</ControlButton>
          <ControlButton onClick={() => sendCommand("right")}>▶</ControlButton>
        </div>
        <div className="row">
          <ControlButton onClick={() => sendCommand("back")}>▼</ControlButton>
        </div>
      </div>
      <p className="status">{status}</p>
      <p className="hint">Usa botones o flechas / WASD</p>
    </div>
  );
}
