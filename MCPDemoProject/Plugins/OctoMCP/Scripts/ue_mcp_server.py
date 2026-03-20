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
SERVER_VERSION = "0.2.0"
UE_HOST = "127.0.0.1"
UE_PORT = 47831
UE_TIMEOUT_SECONDS = 5.0
LIVE_CODING_WAIT_TIMEOUT_SECONDS = 300.0
LIVE_CODING_NOWAIT_TIMEOUT_SECONDS = 15.0
CREATE_WIDGET_BLUEPRINT_TIMEOUT_SECONDS = 30.0
VERSION_TOOL_NAME = "ue_get_version_info"
LIVE_CODING_TOOL_NAME = "ue_live_coding_compile"
CREATE_WIDGET_BLUEPRINT_TOOL_NAME = "ue_create_widget_blueprint"


class JsonRpcError(Exception):
    def __init__(self, code: int, message: str, data: Any | None = None) -> None:
        super().__init__(message)
        self.code = code
        self.message = message
        self.data = data


class UeBridgeError(Exception):
    """Raised when the Unreal Editor bridge cannot satisfy a request."""

    def __init__(self, message: str, editor_reachable: bool) -> None:
        super().__init__(message)
        self.editor_reachable = editor_reachable


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


def build_version_tool_definition() -> dict[str, Any]:
    return {
        "name": VERSION_TOOL_NAME,
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


def build_live_coding_tool_definition() -> dict[str, Any]:
    return {
        "name": LIVE_CODING_TOOL_NAME,
        "title": "Trigger Unreal Live Coding compile",
        "description": (
            "Start a Live Coding compile in the running Unreal Editor and optionally "
            "wait for completion."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "waitForCompletion": {
                    "type": "boolean",
                    "default": True,
                    "description": "Wait until the Live Coding compile finishes before responding.",
                }
            },
            "additionalProperties": False,
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "mcpProtocolVersion": {"type": "string"},
                "liveCodingSupported": {"type": "boolean"},
                "waitedForCompletion": {"type": "boolean"},
                "success": {"type": "boolean"},
                "compileResult": {"type": "string"},
                "message": {"type": "string"},
                "enableError": {"type": "string"},
                "enabledByDefault": {"type": "boolean"},
                "canEnableForSession": {"type": "boolean"},
                "enabledForSession": {"type": "boolean"},
                "hasStarted": {"type": "boolean"},
                "isCompiling": {"type": "boolean"},
                "editorReachable": {"type": "boolean"},
            },
            "required": [
                "mcpProtocolVersion",
                "liveCodingSupported",
                "waitedForCompletion",
                "success",
                "compileResult",
                "message",
                "enableError",
                "enabledByDefault",
                "canEnableForSession",
                "enabledForSession",
                "hasStarted",
                "isCompiling",
                "editorReachable",
            ],
            "additionalProperties": False,
        },
    }


def build_create_widget_blueprint_tool_definition() -> dict[str, Any]:
    return {
        "name": CREATE_WIDGET_BLUEPRINT_TOOL_NAME,
        "title": "Create Unreal Widget Blueprint",
        "description": (
            "Create a Widget Blueprint asset in the running Unreal Editor using the "
            "specified parent UUserWidget-derived class."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "assetPath": {
                    "type": "string",
                    "description": (
                        "Asset package path such as /Game/UI/Widget/WBP_MCPPopup or "
                        "full object path such as /Game/UI/Widget/WBP_MCPPopup.WBP_MCPPopup."
                    ),
                },
                "parentClassPath": {
                    "type": "string",
                    "description": (
                        "Parent widget class path such as /Script/MCPDemoProject.MCPPopupWidget."
                    ),
                },
                "saveAsset": {
                    "type": "boolean",
                    "default": True,
                    "description": "Save the newly created asset to disk before responding.",
                },
            },
            "required": ["assetPath", "parentClassPath"],
            "additionalProperties": False,
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "mcpProtocolVersion": {"type": "string"},
                "created": {"type": "boolean"},
                "saved": {"type": "boolean"},
                "success": {"type": "boolean"},
                "message": {"type": "string"},
                "assetPath": {"type": "string"},
                "assetObjectPath": {"type": "string"},
                "packagePath": {"type": "string"},
                "assetName": {"type": "string"},
                "parentClassPath": {"type": "string"},
                "parentClassName": {"type": "string"},
                "editorReachable": {"type": "boolean"},
            },
            "required": [
                "mcpProtocolVersion",
                "created",
                "saved",
                "success",
                "message",
                "assetPath",
                "assetObjectPath",
                "packagePath",
                "assetName",
                "parentClassPath",
                "parentClassName",
                "editorReachable",
            ],
            "additionalProperties": False,
        },
    }


def require_initialized(method: str) -> None:
    if not STATE.initialized:
        raise JsonRpcError(-32002, f"Server has not received notifications/initialized before {method}.")


def require_tool_arguments(tool_name: str, arguments: Any) -> dict[str, Any]:
    if arguments is None:
        return {}

    if not isinstance(arguments, dict):
        raise JsonRpcError(-32602, f"{tool_name} arguments must be an object.")

    return arguments


def call_ue_bridge(
    command: str, arguments: dict[str, Any] | None = None, timeout_seconds: float = UE_TIMEOUT_SECONDS
) -> dict[str, Any]:
    request_body = json.dumps(
        {
            "command": command,
            "arguments": arguments or {},
            "requestId": str(uuid.uuid4()),
        },
        ensure_ascii=False,
        separators=(",", ":"),
    )

    connection = http.client.HTTPConnection(UE_HOST, UE_PORT, timeout=timeout_seconds)
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
            f"Unable to reach the Unreal Editor bridge at http://{UE_HOST}:{UE_PORT}: {exc}",
            editor_reachable=False,
        ) from exc
    finally:
        connection.close()

    response_text = response_bytes.decode("utf-8", errors="replace")
    try:
        payload = json.loads(response_text) if response_text else {}
    except json.JSONDecodeError as exc:
        raise UeBridgeError(
            f"Unreal Editor bridge returned malformed JSON (HTTP {response.status}): {exc.msg}",
            editor_reachable=True,
        ) from exc

    if response.status != 200:
        error = payload.get("error", {})
        code = error.get("code", "bridge_error")
        message = error.get("message", f"Bridge request failed with HTTP {response.status}")
        raise UeBridgeError(f"{code}: {message}", editor_reachable=True)

    if not payload.get("ok", False):
        error = payload.get("error", {})
        code = error.get("code", "bridge_error")
        message = error.get("message", "Bridge request failed.")
        raise UeBridgeError(f"{code}: {message}", editor_reachable=True)

    result = payload.get("result")
    if not isinstance(result, dict):
        raise UeBridgeError(
            "Unreal Editor bridge response is missing a result object.",
            editor_reachable=True,
        )

    return result


def build_version_tool_success() -> dict[str, Any]:
    bridge_result = call_ue_bridge("get_version_info")
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


def build_version_tool_error(message: str, editor_reachable: bool) -> dict[str, Any]:
    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "engineVersion": "",
        "buildVersion": "",
        "projectName": "",
        "pluginVersion": "",
        "editorReachable": editor_reachable,
    }

    return {
        "content": [{"type": "text", "text": message}],
        "structuredContent": structured_content,
        "isError": True,
    }


def build_live_coding_tool_success(arguments: dict[str, Any]) -> dict[str, Any]:
    wait_for_completion = arguments.get("waitForCompletion", True)
    if not isinstance(wait_for_completion, bool):
        raise JsonRpcError(-32602, "ue_live_coding_compile.waitForCompletion must be a boolean.")

    bridge_result = call_ue_bridge(
        "live_coding_compile",
        {"waitForCompletion": wait_for_completion},
        timeout_seconds=(
            LIVE_CODING_WAIT_TIMEOUT_SECONDS if wait_for_completion else LIVE_CODING_NOWAIT_TIMEOUT_SECONDS
        ),
    )

    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "liveCodingSupported": bool(bridge_result.get("liveCodingSupported", False)),
        "waitedForCompletion": bool(bridge_result.get("waitedForCompletion", wait_for_completion)),
        "success": bool(bridge_result.get("success", False)),
        "compileResult": str(bridge_result.get("compileResult", "")),
        "message": str(bridge_result.get("message", "")),
        "enableError": str(bridge_result.get("enableError", "")),
        "enabledByDefault": bool(bridge_result.get("enabledByDefault", False)),
        "canEnableForSession": bool(bridge_result.get("canEnableForSession", False)),
        "enabledForSession": bool(bridge_result.get("enabledForSession", False)),
        "hasStarted": bool(bridge_result.get("hasStarted", False)),
        "isCompiling": bool(bridge_result.get("isCompiling", False)),
        "editorReachable": True,
    }

    summary = (
        f"{structured_content['compileResult']} | "
        f"success={structured_content['success']} | "
        f"started={structured_content['hasStarted']} | "
        f"enabled={structured_content['enabledForSession']} | "
        f"{structured_content['message']}"
    )

    return {
        "content": [{"type": "text", "text": summary}],
        "structuredContent": structured_content,
        "isError": not structured_content["success"],
    }


def build_live_coding_tool_error(message: str, editor_reachable: bool, wait_for_completion: bool) -> dict[str, Any]:
    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "liveCodingSupported": False,
        "waitedForCompletion": wait_for_completion,
        "success": False,
        "compileResult": "",
        "message": message,
        "enableError": "",
        "enabledByDefault": False,
        "canEnableForSession": False,
        "enabledForSession": False,
        "hasStarted": False,
        "isCompiling": False,
        "editorReachable": editor_reachable,
    }

    return {
        "content": [{"type": "text", "text": message}],
        "structuredContent": structured_content,
        "isError": True,
    }


def build_create_widget_blueprint_tool_success(arguments: dict[str, Any]) -> dict[str, Any]:
    asset_path = arguments.get("assetPath")
    if not isinstance(asset_path, str) or not asset_path.strip():
        raise JsonRpcError(-32602, "ue_create_widget_blueprint.assetPath must be a non-empty string.")

    parent_class_path = arguments.get("parentClassPath")
    if not isinstance(parent_class_path, str) or not parent_class_path.strip():
        raise JsonRpcError(-32602, "ue_create_widget_blueprint.parentClassPath must be a non-empty string.")

    save_asset = arguments.get("saveAsset", True)
    if not isinstance(save_asset, bool):
        raise JsonRpcError(-32602, "ue_create_widget_blueprint.saveAsset must be a boolean.")

    bridge_result = call_ue_bridge(
        "create_widget_blueprint",
        {
            "assetPath": asset_path,
            "parentClassPath": parent_class_path,
            "saveAsset": save_asset,
        },
        timeout_seconds=CREATE_WIDGET_BLUEPRINT_TIMEOUT_SECONDS,
    )

    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "created": bool(bridge_result.get("created", False)),
        "saved": bool(bridge_result.get("saved", False)),
        "success": bool(bridge_result.get("success", False)),
        "message": str(bridge_result.get("message", "")),
        "assetPath": str(bridge_result.get("assetPath", "")),
        "assetObjectPath": str(bridge_result.get("assetObjectPath", "")),
        "packagePath": str(bridge_result.get("packagePath", "")),
        "assetName": str(bridge_result.get("assetName", "")),
        "parentClassPath": str(bridge_result.get("parentClassPath", "")),
        "parentClassName": str(bridge_result.get("parentClassName", "")),
        "editorReachable": True,
    }

    summary = (
        f"created={structured_content['created']} | "
        f"saved={structured_content['saved']} | "
        f"asset={structured_content['assetObjectPath'] or structured_content['assetPath']} | "
        f"parent={structured_content['parentClassPath']} | "
        f"{structured_content['message']}"
    )

    return {
        "content": [{"type": "text", "text": summary}],
        "structuredContent": structured_content,
        "isError": not structured_content["success"],
    }


def build_create_widget_blueprint_tool_error(
    message: str, editor_reachable: bool, asset_path: str, parent_class_path: str
) -> dict[str, Any]:
    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "created": False,
        "saved": False,
        "success": False,
        "message": message,
        "assetPath": asset_path,
        "assetObjectPath": "",
        "packagePath": "",
        "assetName": "",
        "parentClassPath": parent_class_path,
        "parentClassName": "",
        "editorReachable": editor_reachable,
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
                "Call ue_get_version_info to verify connectivity, or "
                "ue_live_coding_compile to trigger a Live Coding build in the running Unreal Editor, "
                "or ue_create_widget_blueprint to generate a Widget Blueprint asset from a parent class."
            ),
        },
    )


def handle_tools_list(message_id: Any) -> dict[str, Any]:
    require_initialized("tools/list")
    return make_response(
        message_id,
        {
            "tools": [
                build_version_tool_definition(),
                build_live_coding_tool_definition(),
                build_create_widget_blueprint_tool_definition(),
            ]
        },
    )


def handle_tools_call(message_id: Any, params: Any) -> dict[str, Any]:
    require_initialized("tools/call")
    if not isinstance(params, dict):
        raise JsonRpcError(-32602, "tools/call params must be an object.")

    tool_name = params.get("name")
    if not isinstance(tool_name, str):
        raise JsonRpcError(-32602, "tools/call name must be a string.")

    tool_arguments = require_tool_arguments(tool_name, params.get("arguments", {}))

    if tool_name == VERSION_TOOL_NAME:
        try:
            result = build_version_tool_success()
        except UeBridgeError as exc:
            result = build_version_tool_error(str(exc), exc.editor_reachable)
        return make_response(message_id, result)

    if tool_name == LIVE_CODING_TOOL_NAME:
        wait_for_completion = tool_arguments.get("waitForCompletion", True)
        if not isinstance(wait_for_completion, bool):
            raise JsonRpcError(-32602, "ue_live_coding_compile.waitForCompletion must be a boolean.")
        try:
            result = build_live_coding_tool_success(tool_arguments)
        except UeBridgeError as exc:
            result = build_live_coding_tool_error(str(exc), exc.editor_reachable, wait_for_completion)
        return make_response(message_id, result)

    if tool_name == CREATE_WIDGET_BLUEPRINT_TOOL_NAME:
        asset_path = tool_arguments.get("assetPath", "")
        parent_class_path = tool_arguments.get("parentClassPath", "")
        if asset_path is not None and not isinstance(asset_path, str):
            raise JsonRpcError(-32602, "ue_create_widget_blueprint.assetPath must be a string.")
        if parent_class_path is not None and not isinstance(parent_class_path, str):
            raise JsonRpcError(-32602, "ue_create_widget_blueprint.parentClassPath must be a string.")
        try:
            result = build_create_widget_blueprint_tool_success(tool_arguments)
        except UeBridgeError as exc:
            result = build_create_widget_blueprint_tool_error(
                str(exc),
                exc.editor_reachable,
                str(asset_path or ""),
                str(parent_class_path or ""),
            )
        return make_response(message_id, result)

    raise JsonRpcError(-32602, f"Unknown tool: {tool_name!r}")


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
