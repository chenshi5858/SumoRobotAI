import { useEffect, useRef, useState } from "react";

const AXIS_DEADZONE = 0.45;
const BUTTON_THRESHOLD = 0.5;

const BUTTONS = {
  cross: 0,
  l1: 4,
  r1: 5,
  dpadUp: 12,
  dpadDown: 13,
  dpadLeft: 14,
  dpadRight: 15,
};

const IDENTITY_DIRECTIONS = {
  forward: "forward",
  right: "right",
  back: "back",
  left: "left",
};

function canUseGamepadApi() {
  if (!("getGamepads" in navigator)) {
    return "Gamepad API no disponible en este navegador";
  }
  if (!window.isSecureContext) {
    return "Gamepad API bloqueada: abre la web en localhost o HTTPS";
  }
  return "";
}

function isButtonPressed(gamepad, index) {
  const button = gamepad.buttons[index];
  return Boolean(button && (button.pressed || button.value > BUTTON_THRESHOLD));
}

function isLikelyPs4Controller(gamepad) {
  return /dualshock|playstation|ps4|wireless controller|sony/i.test(gamepad.id);
}

function getGamepads() {
  try {
    return Array.from(navigator.getGamepads()).filter(Boolean);
  } catch (error) {
    console.warn("No se pudo leer navigator.getGamepads()", error);
    return [];
  }
}

function findController() {
  const gamepads = getGamepads().filter((gamepad) => gamepad.connected);
  return (
    gamepads.find(isLikelyPs4Controller) ||
    gamepads.find((gamepad) => gamepad.mapping === "standard") ||
    gamepads[0] ||
    null
  );
}

function readLogicalDirections(gamepad) {
  const directions = new Set();
  const xAxis = gamepad.axes[0] || 0;
  const yAxis = gamepad.axes[1] || 0;
  const dpadXAxis = gamepad.axes[6] || 0;
  const dpadYAxis = gamepad.axes[7] || 0;

  if (
    yAxis < -AXIS_DEADZONE ||
    dpadYAxis < -AXIS_DEADZONE ||
    isButtonPressed(gamepad, BUTTONS.dpadUp)
  ) {
    directions.add("forward");
  }
  if (
    yAxis > AXIS_DEADZONE ||
    dpadYAxis > AXIS_DEADZONE ||
    isButtonPressed(gamepad, BUTTONS.dpadDown)
  ) {
    directions.add("back");
  }
  if (
    xAxis < -AXIS_DEADZONE ||
    dpadXAxis < -AXIS_DEADZONE ||
    isButtonPressed(gamepad, BUTTONS.dpadLeft)
  ) {
    directions.add("left");
  }
  if (
    xAxis > AXIS_DEADZONE ||
    dpadXAxis > AXIS_DEADZONE ||
    isButtonPressed(gamepad, BUTTONS.dpadRight)
  ) {
    directions.add("right");
  }

  return directions;
}

function resolveDirection(logicalDirections, directionCommands) {
  const commandDirections = new Set();

  logicalDirections.forEach((direction) => {
    commandDirections.add(directionCommands[direction] || direction);
  });

  const hasForward = commandDirections.has("forward");
  const hasBack = commandDirections.has("back");
  const hasLeft = commandDirections.has("left");
  const hasRight = commandDirections.has("right");

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

export function usePs4Controller({
  directionCommands = IDENTITY_DIRECTIONS,
  onMoveStart,
  onMoveStop,
  onSpeedDown,
  onSpeedUp,
}) {
  const [controllerName, setControllerName] = useState("");
  const [controllerStatus, setControllerStatus] = useState(
    "PS4: presiona un boton del control con la pagina abierta"
  );
  const [isControllerConnected, setIsControllerConnected] = useState(false);
  const callbacksRef = useRef({
    directionCommands,
    onMoveStart,
    onMoveStop,
    onSpeedDown,
    onSpeedUp,
  });
  const connectedRef = useRef(false);
  const activeDirectionRef = useRef(null);
  const previousButtonsRef = useRef({
    stop: false,
    speedDown: false,
    speedUp: false,
  });

  useEffect(() => {
    callbacksRef.current = {
      directionCommands,
      onMoveStart,
      onMoveStop,
      onSpeedDown,
      onSpeedUp,
    };
  }, [directionCommands, onMoveStart, onMoveStop, onSpeedDown, onSpeedUp]);

  useEffect(() => {
    const apiError = canUseGamepadApi();
    if (apiError) {
      setControllerStatus(apiError);
      return undefined;
    }

    let animationFrameId = 0;

    function setConnectionState(isConnected, name = "") {
      if (connectedRef.current === isConnected) return;
      connectedRef.current = isConnected;
      setIsControllerConnected(isConnected);
      setControllerName(name);
      setControllerStatus(
        isConnected
          ? `PS4 conectado: ${name || "control detectado"}`
          : "PS4: presiona un boton del control con la pagina abierta"
      );
    }

    function stopActiveMovement() {
      if (!activeDirectionRef.current) return;
      activeDirectionRef.current = null;
      callbacksRef.current.onMoveStop();
    }

    function pollController() {
      const controller = findController();

      if (!controller) {
        stopActiveMovement();
        setConnectionState(false);
        previousButtonsRef.current = {
          stop: false,
          speedDown: false,
          speedUp: false,
        };
        animationFrameId = requestAnimationFrame(pollController);
        return;
      }

      setConnectionState(true, controller.id);

      const buttons = {
        stop: isButtonPressed(controller, BUTTONS.cross),
        speedDown: isButtonPressed(controller, BUTTONS.l1),
        speedUp: isButtonPressed(controller, BUTTONS.r1),
      };
      const previousButtons = previousButtonsRef.current;

      if (buttons.speedDown && !previousButtons.speedDown) {
        callbacksRef.current.onSpeedDown();
      }
      if (buttons.speedUp && !previousButtons.speedUp) {
        callbacksRef.current.onSpeedUp();
      }
      if (buttons.stop && !previousButtons.stop) {
        activeDirectionRef.current = null;
        callbacksRef.current.onMoveStop();
      }

      const nextDirection = buttons.stop
        ? null
        : resolveDirection(
            readLogicalDirections(controller),
            callbacksRef.current.directionCommands
          );

      if (nextDirection !== activeDirectionRef.current) {
        if (nextDirection) {
          callbacksRef.current.onMoveStart(nextDirection);
        } else if (activeDirectionRef.current) {
          callbacksRef.current.onMoveStop();
        }
        activeDirectionRef.current = nextDirection;
      }

      previousButtonsRef.current = buttons;
      animationFrameId = requestAnimationFrame(pollController);
    }

    function handleDisconnect() {
      stopActiveMovement();
      setConnectionState(false);
    }

    function handleConnect(event) {
      setConnectionState(true, event.gamepad.id);
    }

    window.addEventListener("gamepadconnected", handleConnect);
    window.addEventListener("gamepaddisconnected", handleDisconnect);
    animationFrameId = requestAnimationFrame(pollController);

    return () => {
      window.removeEventListener("gamepadconnected", handleConnect);
      window.removeEventListener("gamepaddisconnected", handleDisconnect);
      cancelAnimationFrame(animationFrameId);
      stopActiveMovement();
    };
  }, []);

  return { controllerName, controllerStatus, isControllerConnected };
}
