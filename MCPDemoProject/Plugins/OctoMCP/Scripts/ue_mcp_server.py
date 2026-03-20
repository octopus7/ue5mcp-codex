#!/usr/bin/env python3
"""OctoMCP stdio MCP server.

This process speaks newline-delimited JSON-RPC over stdin/stdout and forwards
tool calls to the OctoMCP Unreal Editor plugin over localhost HTTP.
"""

from __future__ import annotations

import http.client
import json
import sys
import traceback
import uuid
from dataclasses import dataclass
from typing import Any


MCP_PROTOCOL_VERSION = "2025-11-25"
SERVER_NAME = "OctoMCP"
SERVER_VERSION = "0.1.0"
UE_HOST = "127.0.0.1"
UE_PORT = 47831
UE_TIMEOUT_SECONDS = 5.0
TOOL_NAME = "ue_get_version_info"


class JsonRpcError(Exception):
    def __init__(self, code: int, message: str, data: Any | None = None) -> None:
        super().__init__(message)
        self.code = code
        self.message = message
        self.data = data


class UeBridgeError(Exception):
    """Raised when the Unreal Editor bridge cannot satisfy a request."""


@dataclass
class ServerState:
    initialized: bool = False


STATE = ServerState()


def log(message: str) -> None:
    print(message, file=sys.stderr, flush=True)


def send_message(message: dict[str, Any]) -> None:
    payload = json.dumps(message, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    sys.stdout.buffer.write(payload + b"\n")
    sys.stdout.buffer.flush()


def make_response(message_id: Any, result: dict[str, Any]) -> dict[str, Any]:
    return {"jsonrpc": "2.0", "id": message_id, "result": result}


def make_error(message_id: Any, code: int, message: str, data: Any | None = None) -> dict[str, Any]:
    error: dict[str, Any] = {"code": code, "message": message}
    if data is not None:
        error["data"] = data
    return {"jsonrpc": "2.0", "id": message_id, "error": error}


def read_message(raw_line: bytes) -> Any:
    if not raw_line.strip():
        raise JsonRpcError(-32700, "Blank lines are not valid MCP messages.")

    try:
        return json.loads(raw_line.decode("utf-8"))
    except UnicodeDecodeError as exc:
        raise JsonRpcError(-32700, f"Invalid UTF-8 input: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise JsonRpcError(-32700, f"Invalid JSON input: {exc.msg}") from exc


def build_tool_definition() -> dict[str, Any]:
    return {
        "name": TOOL_NAME,
        "title": "Get Unreal Editor version info",
        "description": (
            "Fetch the running Unreal Editor's engine version, build version, "
            "project name, and OctoMCP plugin version."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {},
            "additionalProperties": False,
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "mcpProtocolVersion": {"type": "string"},
                "engineVersion": {"type": "string"},
                "buildVersion": {"type": "string"},
                "projectName": {"type": "string"},
                "pluginVersion": {"type": "string"},
                "editorReachable": {"type": "boolean"},
            },
            "required": [
                "mcpProtocolVersion",
                "engineVersion",
                "buildVersion",
                "projectName",
                "pluginVersion",
                "editorReachable",
            ],
            "additionalProperties": False,
        },
    }


def require_initialized(method: str) -> None:
    if not STATE.initialized:
        raise JsonRpcError(-32002, f"Server has not received notifications/initialized before {method}.")


def call_ue_bridge() -> dict[str, Any]:
    request_body = json.dumps(
        {
            "command": "get_version_info",
            "arguments": {},
            "requestId": str(uuid.uuid4()),
        },
        ensure_ascii=False,
        separators=(",", ":"),
    )

    connection = http.client.HTTPConnection(UE_HOST, UE_PORT, timeout=UE_TIMEOUT_SECONDS)
    try:
        connection.request(
            "POST",
            "/api/v1/command",
            body=request_body.encode("utf-8"),
            headers={
                "Accept": "application/json",
                "Content-Type": "application/json",
            },
        )
        response = connection.getresponse()
        response_bytes = response.read()
    except OSError as exc:
        raise UeBridgeError(
            f"Unable to reach the Unreal Editor bridge at http://{UE_HOST}:{UE_PORT}: {exc}"
        ) from exc
    finally:
        connection.close()

    response_text = response_bytes.decode("utf-8", errors="replace")
    try:
        payload = json.loads(response_text) if response_text else {}
    except json.JSONDecodeError as exc:
        raise UeBridgeError(
            f"Unreal Editor bridge returned malformed JSON (HTTP {response.status}): {exc.msg}"
        ) from exc

    if response.status != 200:
        error = payload.get("error", {})
        code = error.get("code", "bridge_error")
        message = error.get("message", f"Bridge request failed with HTTP {response.status}")
        raise UeBridgeError(f"{code}: {message}")

    if not payload.get("ok", False):
        error = payload.get("error", {})
        code = error.get("code", "bridge_error")
        message = error.get("message", "Bridge request failed.")
        raise UeBridgeError(f"{code}: {message}")

    result = payload.get("result")
    if not isinstance(result, dict):
        raise UeBridgeError("Unreal Editor bridge response is missing a result object.")

    return result


def build_tool_success() -> dict[str, Any]:
    bridge_result = call_ue_bridge()
    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "engineVersion": str(bridge_result.get("engineVersion", "")),
        "buildVersion": str(bridge_result.get("buildVersion", "")),
        "projectName": str(bridge_result.get("projectName", "")),
        "pluginVersion": str(bridge_result.get("pluginVersion", "")),
        "editorReachable": True,
    }

    summary = (
        f"MCP {structured_content['mcpProtocolVersion']} | "
        f"UE {structured_content['engineVersion']} | "
        f"Build {structured_content['buildVersion']} | "
        f"Project {structured_content['projectName']} | "
        f"Plugin {structured_content['pluginVersion']}"
    )

    return {
        "content": [{"type": "text", "text": summary}],
        "structuredContent": structured_content,
        "isError": False,
    }


def build_tool_error(message: str) -> dict[str, Any]:
    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "engineVersion": "",
        "buildVersion": "",
        "projectName": "",
        "pluginVersion": "",
        "editorReachable": False,
    }

    return {
        "content": [{"type": "text", "text": message}],
        "structuredContent": structured_content,
        "isError": True,
    }


def handle_initialize(message_id: Any, params: Any) -> dict[str, Any]:
    if not isinstance(params, dict):
        raise JsonRpcError(-32602, "initialize params must be an object.")

    requested_version = params.get("protocolVersion")
    if not isinstance(requested_version, str):
        raise JsonRpcError(-32602, "initialize.protocolVersion must be a string.")

    return make_response(
        message_id,
        {
            "protocolVersion": MCP_PROTOCOL_VERSION,
            "capabilities": {
                "tools": {},
            },
            "serverInfo": {
                "name": SERVER_NAME,
                "title": SERVER_NAME,
                "version": SERVER_VERSION,
            },
            "instructions": (
                "Call ue_get_version_info to verify connectivity between the stdio MCP "
                "server and the running Unreal Editor bridge."
            ),
        },
    )


def handle_tools_list(message_id: Any) -> dict[str, Any]:
    require_initialized("tools/list")
    return make_response(message_id, {"tools": [build_tool_definition()]})


def handle_tools_call(message_id: Any, params: Any) -> dict[str, Any]:
    require_initialized("tools/call")
    if not isinstance(params, dict):
        raise JsonRpcError(-32602, "tools/call params must be an object.")

    tool_name = params.get("name")
    if tool_name != TOOL_NAME:
        raise JsonRpcError(-32602, f"Unknown tool: {tool_name!r}")

    try:
        result = build_tool_success()
    except UeBridgeError as exc:
        result = build_tool_error(str(exc))

    return make_response(message_id, result)


def handle_ping(message_id: Any) -> dict[str, Any]:
    return make_response(message_id, {})


def handle_request(message: dict[str, Any]) -> dict[str, Any] | None:
    message_id = message.get("id")
    method = message.get("method")

    if not isinstance(method, str):
        if message_id is None:
            return None
        raise JsonRpcError(-32600, "JSON-RPC request is missing a string method.")

    params = message.get("params", {})

    if method == "initialize":
        return handle_initialize(message_id, params)

    if method == "notifications/initialized":
        STATE.initialized = True
        return None

    if method == "ping":
        return handle_ping(message_id)

    if method == "tools/list":
        return handle_tools_list(message_id)

    if method == "tools/call":
        return handle_tools_call(message_id, params)

    if method.startswith("notifications/"):
        return None

    raise JsonRpcError(-32601, f"Method not found: {method}")


def process_raw_line(raw_line: bytes) -> None:
    parsed = read_message(raw_line)
    if not isinstance(parsed, dict):
        raise JsonRpcError(-32600, "Top-level JSON-RPC message must be an object.")

    response = handle_request(parsed)
    if response is not None:
        send_message(response)


def main() -> int:
    log(f"{SERVER_NAME} stdio server starting on MCP protocol {MCP_PROTOCOL_VERSION}")

    for raw_line in sys.stdin.buffer:
        try:
            process_raw_line(raw_line)
        except JsonRpcError as exc:
            message_id = None
            try:
                maybe_message = json.loads(raw_line.decode("utf-8"))
                if isinstance(maybe_message, dict):
                    message_id = maybe_message.get("id")
            except Exception:
                message_id = None
            send_message(make_error(message_id, exc.code, exc.message, exc.data))
        except Exception as exc:  # pragma: no cover - defensive fallback
            log("Unhandled exception:\n" + traceback.format_exc())
            send_message(make_error(None, -32603, f"Internal server error: {exc}"))

    log(f"{SERVER_NAME} stdio server shutting down")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
