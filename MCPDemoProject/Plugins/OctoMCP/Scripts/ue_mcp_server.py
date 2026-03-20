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
CREATE_BLUEPRINT_ASSET_TIMEOUT_SECONDS = 30.0
CREATE_WIDGET_BLUEPRINT_TIMEOUT_SECONDS = 30.0
IMPORT_TEXTURE_ASSET_TIMEOUT_SECONDS = 60.0
REORDER_WIDGET_CHILD_TIMEOUT_SECONDS = 30.0
REMOVE_WIDGET_TIMEOUT_SECONDS = 30.0
SCAFFOLD_WIDGET_BLUEPRINT_TIMEOUT_SECONDS = 30.0
SET_BLUEPRINT_CLASS_PROPERTY_TIMEOUT_SECONDS = 30.0
SET_GLOBAL_DEFAULT_GAME_MODE_TIMEOUT_SECONDS = 15.0
SET_WIDGET_BACKGROUND_BLUR_TIMEOUT_SECONDS = 30.0
SET_WIDGET_CORNER_RADIUS_TIMEOUT_SECONDS = 30.0
SET_WIDGET_IMAGE_TEXTURE_TIMEOUT_SECONDS = 30.0
SET_WIDGET_PANEL_COLOR_TIMEOUT_SECONDS = 30.0
SET_POPUP_OPEN_ELASTIC_SCALE_TIMEOUT_SECONDS = 30.0
VERSION_TOOL_NAME = "ue_get_version_info"
LIVE_CODING_TOOL_NAME = "ue_live_coding_compile"
CREATE_BLUEPRINT_ASSET_TOOL_NAME = "ue_create_blueprint_asset"
CREATE_WIDGET_BLUEPRINT_TOOL_NAME = "ue_create_widget_blueprint"
IMPORT_TEXTURE_ASSET_TOOL_NAME = "ue_import_texture_asset"
REORDER_WIDGET_CHILD_TOOL_NAME = "ue_reorder_widget_child"
REMOVE_WIDGET_TOOL_NAME = "ue_remove_widget"
SCAFFOLD_WIDGET_BLUEPRINT_TOOL_NAME = "ue_scaffold_widget_blueprint"
SET_BLUEPRINT_CLASS_PROPERTY_TOOL_NAME = "ue_set_blueprint_class_property"
SET_GLOBAL_DEFAULT_GAME_MODE_TOOL_NAME = "ue_set_global_default_game_mode"
SET_WIDGET_BACKGROUND_BLUR_TOOL_NAME = "ue_set_widget_background_blur"
SET_WIDGET_CORNER_RADIUS_TOOL_NAME = "ue_set_widget_corner_radius"
SET_WIDGET_IMAGE_TEXTURE_TOOL_NAME = "ue_set_widget_image_texture"
SET_WIDGET_PANEL_COLOR_TOOL_NAME = "ue_set_widget_panel_color"
SET_POPUP_OPEN_ELASTIC_SCALE_TOOL_NAME = "ue_set_popup_open_elastic_scale"


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


def build_create_blueprint_asset_tool_definition() -> dict[str, Any]:
    return {
        "name": CREATE_BLUEPRINT_ASSET_TOOL_NAME,
        "title": "Create Unreal Blueprint asset",
        "description": (
            "Create a Blueprint asset in the running Unreal Editor using the specified "
            "parent class. Use this for non-UMG Blueprint assets such as GameMode Blueprints."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "assetPath": {
                    "type": "string",
                    "description": (
                        "Asset package path such as /Game/Blueprints/BP_MCPDemoGameMode or "
                        "full object path such as /Game/Blueprints/BP_MCPDemoGameMode.BP_MCPDemoGameMode."
                    ),
                },
                "parentClassPath": {
                    "type": "string",
                    "description": (
                        "Parent class path such as /Script/MCPDemoProject.MCPDemoGameMode."
                    ),
                },
                "saveAsset": {
                    "type": "boolean",
                    "default": True,
                    "description": "Save the newly created Blueprint asset to disk before responding.",
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
                "generatedClassPath": {"type": "string"},
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
                "generatedClassPath",
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


def build_import_texture_asset_tool_definition() -> dict[str, Any]:
    return {
        "name": IMPORT_TEXTURE_ASSET_TOOL_NAME,
        "title": "Import Unreal texture asset",
        "description": (
            "Import a texture file such as PNG into the running Unreal Editor as a project asset."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "sourceFilePath": {
                    "type": "string",
                    "description": "Absolute or relative source image file path on disk.",
                },
                "assetPath": {
                    "type": "string",
                    "description": (
                        "Destination asset package path such as /Game/UI/Image/T_Duck or "
                        "full object path such as /Game/UI/Image/T_Duck.T_Duck."
                    ),
                },
                "replaceExisting": {
                    "type": "boolean",
                    "default": True,
                    "description": "Overwrite an existing asset at the destination path when possible.",
                },
                "saveAsset": {
                    "type": "boolean",
                    "default": True,
                    "description": "Save the imported asset to disk before responding.",
                },
            },
            "required": ["sourceFilePath", "assetPath"],
            "additionalProperties": False,
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "mcpProtocolVersion": {"type": "string"},
                "imported": {"type": "boolean"},
                "saved": {"type": "boolean"},
                "success": {"type": "boolean"},
                "message": {"type": "string"},
                "sourceFilePath": {"type": "string"},
                "assetPath": {"type": "string"},
                "assetObjectPath": {"type": "string"},
                "packagePath": {"type": "string"},
                "assetName": {"type": "string"},
                "editorReachable": {"type": "boolean"},
            },
            "required": [
                "mcpProtocolVersion",
                "imported",
                "saved",
                "success",
                "message",
                "sourceFilePath",
                "assetPath",
                "assetObjectPath",
                "packagePath",
                "assetName",
                "editorReachable",
            ],
            "additionalProperties": False,
        },
    }


def build_reorder_widget_child_tool_definition() -> dict[str, Any]:
    return {
        "name": REORDER_WIDGET_CHILD_TOOL_NAME,
        "title": "Reorder Widget Blueprint child",
        "description": (
            "Reorder a named widget within its current panel parent inside a Widget Blueprint."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "assetPath": {
                    "type": "string",
                    "description": "Widget Blueprint asset path to update.",
                },
                "widgetName": {
                    "type": "string",
                    "description": "Name of the widget to reorder inside the Widget Blueprint.",
                },
                "desiredIndex": {
                    "type": "integer",
                    "description": "Zero-based target index within the widget's current panel parent.",
                },
                "saveAsset": {
                    "type": "boolean",
                    "default": True,
                    "description": "Save the updated Widget Blueprint asset to disk before responding.",
                },
            },
            "required": ["assetPath", "widgetName", "desiredIndex"],
            "additionalProperties": False,
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "mcpProtocolVersion": {"type": "string"},
                "reordered": {"type": "boolean"},
                "saved": {"type": "boolean"},
                "success": {"type": "boolean"},
                "message": {"type": "string"},
                "assetPath": {"type": "string"},
                "assetObjectPath": {"type": "string"},
                "packagePath": {"type": "string"},
                "assetName": {"type": "string"},
                "widgetName": {"type": "string"},
                "parentWidgetName": {"type": "string"},
                "requestedIndex": {"type": "integer"},
                "finalIndex": {"type": "integer"},
                "editorReachable": {"type": "boolean"},
            },
            "required": [
                "mcpProtocolVersion",
                "reordered",
                "saved",
                "success",
                "message",
                "assetPath",
                "assetObjectPath",
                "packagePath",
                "assetName",
                "widgetName",
                "parentWidgetName",
                "requestedIndex",
                "finalIndex",
                "editorReachable",
            ],
            "additionalProperties": False,
        },
    }


def build_remove_widget_tool_definition() -> dict[str, Any]:
    return {
        "name": REMOVE_WIDGET_TOOL_NAME,
        "title": "Remove Widget Blueprint widget",
        "description": (
            "Remove a named widget from a Widget Blueprint widget tree."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "assetPath": {
                    "type": "string",
                    "description": "Widget Blueprint asset path to update.",
                },
                "widgetName": {
                    "type": "string",
                    "description": "Name of the widget to remove from the Widget Blueprint.",
                },
                "saveAsset": {
                    "type": "boolean",
                    "default": True,
                    "description": "Save the updated Widget Blueprint asset to disk before responding.",
                },
            },
            "required": ["assetPath", "widgetName"],
            "additionalProperties": False,
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "mcpProtocolVersion": {"type": "string"},
                "removed": {"type": "boolean"},
                "saved": {"type": "boolean"},
                "success": {"type": "boolean"},
                "message": {"type": "string"},
                "assetPath": {"type": "string"},
                "assetObjectPath": {"type": "string"},
                "packagePath": {"type": "string"},
                "assetName": {"type": "string"},
                "widgetName": {"type": "string"},
                "parentWidgetName": {"type": "string"},
                "editorReachable": {"type": "boolean"},
            },
            "required": [
                "mcpProtocolVersion",
                "removed",
                "saved",
                "success",
                "message",
                "assetPath",
                "assetObjectPath",
                "packagePath",
                "assetName",
                "widgetName",
                "parentWidgetName",
                "editorReachable",
            ],
            "additionalProperties": False,
        },
    }


def build_set_widget_background_blur_tool_definition() -> dict[str, Any]:
    return {
        "name": SET_WIDGET_BACKGROUND_BLUR_TOOL_NAME,
        "title": "Set Widget Blueprint background blur",
        "description": (
            "Ensure a named widget in a Widget Blueprint is a BackgroundBlur widget and configure its blur appearance."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "assetPath": {
                    "type": "string",
                    "description": "Widget Blueprint asset path to update.",
                },
                "widgetName": {
                    "type": "string",
                    "description": "Name of the widget to convert/configure as a BackgroundBlur.",
                },
                "blurStrength": {
                    "type": "number",
                    "default": 30.0,
                    "description": "Blur intensity from 0 to 100.",
                },
                "blurRadius": {
                    "type": "integer",
                    "description": "Optional explicit blur radius. If omitted, Unreal auto-calculates it from the strength.",
                },
                "applyAlphaToBlur": {
                    "type": "boolean",
                    "default": False,
                    "description": "Modulate blur strength by the widget alpha.",
                },
                "saveAsset": {
                    "type": "boolean",
                    "default": True,
                    "description": "Save the updated Widget Blueprint asset to disk before responding.",
                },
            },
            "required": ["assetPath", "widgetName"],
            "additionalProperties": False,
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "mcpProtocolVersion": {"type": "string"},
                "converted": {"type": "boolean"},
                "saved": {"type": "boolean"},
                "success": {"type": "boolean"},
                "message": {"type": "string"},
                "assetPath": {"type": "string"},
                "assetObjectPath": {"type": "string"},
                "packagePath": {"type": "string"},
                "assetName": {"type": "string"},
                "widgetName": {"type": "string"},
                "widgetClassName": {"type": "string"},
                "blurStrength": {"type": "number"},
                "blurRadius": {"type": "integer"},
                "applyAlphaToBlur": {"type": "boolean"},
                "overrideAutoRadiusCalculation": {"type": "boolean"},
                "editorReachable": {"type": "boolean"},
            },
            "required": [
                "mcpProtocolVersion",
                "converted",
                "saved",
                "success",
                "message",
                "assetPath",
                "assetObjectPath",
                "packagePath",
                "assetName",
                "widgetName",
                "widgetClassName",
                "blurStrength",
                "blurRadius",
                "applyAlphaToBlur",
                "overrideAutoRadiusCalculation",
                "editorReachable",
            ],
            "additionalProperties": False,
        },
    }


def build_set_widget_corner_radius_tool_definition() -> dict[str, Any]:
    return {
        "name": SET_WIDGET_CORNER_RADIUS_TOOL_NAME,
        "title": "Set Widget Blueprint corner radius",
        "description": (
            "Set a uniform corner radius on a named BackgroundBlur or Border widget inside a Widget Blueprint."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "assetPath": {
                    "type": "string",
                    "description": "Widget Blueprint asset path to update.",
                },
                "widgetName": {
                    "type": "string",
                    "description": "Name of the target widget.",
                },
                "radius": {
                    "type": "number",
                    "description": "Uniform corner radius in Slate units.",
                },
                "saveAsset": {
                    "type": "boolean",
                    "default": True,
                    "description": "Save the updated Widget Blueprint asset to disk before responding.",
                },
            },
            "required": ["assetPath", "widgetName", "radius"],
            "additionalProperties": False,
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "mcpProtocolVersion": {"type": "string"},
                "saved": {"type": "boolean"},
                "success": {"type": "boolean"},
                "message": {"type": "string"},
                "assetPath": {"type": "string"},
                "assetObjectPath": {"type": "string"},
                "packagePath": {"type": "string"},
                "assetName": {"type": "string"},
                "widgetName": {"type": "string"},
                "widgetClassName": {"type": "string"},
                "radius": {"type": "number"},
                "editorReachable": {"type": "boolean"},
            },
            "required": [
                "mcpProtocolVersion",
                "saved",
                "success",
                "message",
                "assetPath",
                "assetObjectPath",
                "packagePath",
                "assetName",
                "widgetName",
                "widgetClassName",
                "radius",
                "editorReachable",
            ],
            "additionalProperties": False,
        },
    }


def build_set_widget_panel_color_tool_definition() -> dict[str, Any]:
    return {
        "name": SET_WIDGET_PANEL_COLOR_TOOL_NAME,
        "title": "Set Widget Blueprint panel color",
        "description": (
            "Set the RGBA color of a named Border, Button, or Image widget inside a Widget Blueprint. "
            "Channel values may be passed as 0..1 or 0..255."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "assetPath": {
                    "type": "string",
                    "description": "Widget Blueprint asset path to update.",
                },
                "widgetName": {
                    "type": "string",
                    "description": "Name of the target widget.",
                },
                "red": {
                    "type": "number",
                    "description": "Red channel, 0..1 or 0..255.",
                },
                "green": {
                    "type": "number",
                    "description": "Green channel, 0..1 or 0..255.",
                },
                "blue": {
                    "type": "number",
                    "description": "Blue channel, 0..1 or 0..255.",
                },
                "alpha": {
                    "type": "number",
                    "description": "Alpha channel, 0..1 or 0..255.",
                },
                "saveAsset": {
                    "type": "boolean",
                    "default": True,
                    "description": "Save the updated Widget Blueprint asset to disk before responding.",
                },
            },
            "required": ["assetPath", "widgetName", "red", "green", "blue", "alpha"],
            "additionalProperties": False,
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "mcpProtocolVersion": {"type": "string"},
                "saved": {"type": "boolean"},
                "success": {"type": "boolean"},
                "message": {"type": "string"},
                "assetPath": {"type": "string"},
                "assetObjectPath": {"type": "string"},
                "packagePath": {"type": "string"},
                "assetName": {"type": "string"},
                "widgetName": {"type": "string"},
                "widgetClassName": {"type": "string"},
                "red": {"type": "number"},
                "green": {"type": "number"},
                "blue": {"type": "number"},
                "alpha": {"type": "number"},
                "editorReachable": {"type": "boolean"},
            },
            "required": [
                "mcpProtocolVersion",
                "saved",
                "success",
                "message",
                "assetPath",
                "assetObjectPath",
                "packagePath",
                "assetName",
                "widgetName",
                "widgetClassName",
                "red",
                "green",
                "blue",
                "alpha",
                "editorReachable",
            ],
            "additionalProperties": False,
        },
    }


def build_set_popup_open_elastic_scale_tool_definition() -> dict[str, Any]:
    return {
        "name": SET_POPUP_OPEN_ELASTIC_SCALE_TOOL_NAME,
        "title": "Set popup open elastic scale",
        "description": (
            "Configure a UMCPPopupWidget-derived Widget Blueprint so its popup panel plays an elastic scale "
            "animation when opened."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "assetPath": {
                    "type": "string",
                    "description": "Widget Blueprint asset path to update.",
                },
                "enabled": {
                    "type": "boolean",
                    "default": True,
                    "description": "Enable or disable the popup open elastic scale animation.",
                },
                "widgetName": {
                    "type": "string",
                    "default": "PopupCard",
                    "description": "Name of the widget inside the popup to animate.",
                },
                "duration": {
                    "type": "number",
                    "default": 0.45,
                    "description": "Animation duration in seconds.",
                },
                "startScale": {
                    "type": "number",
                    "default": 0.82,
                    "description": "Initial uniform scale before the elastic animation settles to 1.0.",
                },
                "oscillationCount": {
                    "type": "number",
                    "default": 2.0,
                    "description": "How many elastic oscillations occur before the scale settles.",
                },
                "pivotX": {
                    "type": "number",
                    "default": 0.5,
                    "description": "Render transform pivot X in 0..1 space.",
                },
                "pivotY": {
                    "type": "number",
                    "default": 0.5,
                    "description": "Render transform pivot Y in 0..1 space.",
                },
                "saveAsset": {
                    "type": "boolean",
                    "default": True,
                    "description": "Save the updated Widget Blueprint asset to disk before responding.",
                },
            },
            "required": ["assetPath"],
            "additionalProperties": False,
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "mcpProtocolVersion": {"type": "string"},
                "saved": {"type": "boolean"},
                "success": {"type": "boolean"},
                "enabled": {"type": "boolean"},
                "message": {"type": "string"},
                "assetPath": {"type": "string"},
                "assetObjectPath": {"type": "string"},
                "packagePath": {"type": "string"},
                "assetName": {"type": "string"},
                "widgetName": {"type": "string"},
                "duration": {"type": "number"},
                "startScale": {"type": "number"},
                "oscillationCount": {"type": "number"},
                "pivotX": {"type": "number"},
                "pivotY": {"type": "number"},
                "editorReachable": {"type": "boolean"},
            },
            "required": [
                "mcpProtocolVersion",
                "saved",
                "success",
                "enabled",
                "message",
                "assetPath",
                "assetObjectPath",
                "packagePath",
                "assetName",
                "widgetName",
                "duration",
                "startScale",
                "oscillationCount",
                "pivotX",
                "pivotY",
                "editorReachable",
            ],
            "additionalProperties": False,
        },
    }


def build_scaffold_widget_blueprint_tool_definition() -> dict[str, Any]:
    return {
        "name": SCAFFOLD_WIDGET_BLUEPRINT_TOOL_NAME,
        "title": "Scaffold Unreal Widget Blueprint",
        "description": (
            "Populate an existing Widget Blueprint asset with a predefined widget-tree scaffold. "
            "Currently supports popup and bottom button bar scaffolds."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "assetPath": {
                    "type": "string",
                    "description": (
                        "Existing Widget Blueprint asset path such as /Game/UI/Widget/WBP_MCPPopup "
                        "or /Game/UI/Widget/WBP_MCPPopup.WBP_MCPPopup."
                    ),
                },
                "scaffoldType": {
                    "type": "string",
                    "enum": ["popup", "bottom_button_bar"],
                    "description": "Predefined widget tree scaffold to apply to the target Widget Blueprint.",
                },
                "saveAsset": {
                    "type": "boolean",
                    "default": True,
                    "description": "Save the updated asset to disk before responding.",
                },
            },
            "required": ["assetPath", "scaffoldType"],
            "additionalProperties": False,
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "mcpProtocolVersion": {"type": "string"},
                "saved": {"type": "boolean"},
                "success": {"type": "boolean"},
                "message": {"type": "string"},
                "assetPath": {"type": "string"},
                "assetObjectPath": {"type": "string"},
                "packagePath": {"type": "string"},
                "assetName": {"type": "string"},
                "scaffoldType": {"type": "string"},
                "editorReachable": {"type": "boolean"},
            },
            "required": [
                "mcpProtocolVersion",
                "saved",
                "success",
                "message",
                "assetPath",
                "assetObjectPath",
                "packagePath",
                "assetName",
                "scaffoldType",
                "editorReachable",
            ],
            "additionalProperties": False,
        },
    }


def build_set_blueprint_class_property_tool_definition() -> dict[str, Any]:
    return {
        "name": SET_BLUEPRINT_CLASS_PROPERTY_TOOL_NAME,
        "title": "Set Blueprint class property",
        "description": (
            "Set a Blueprint or Widget Blueprint class-reference default property to a target class."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "assetPath": {
                    "type": "string",
                    "description": "Blueprint asset path to update.",
                },
                "propertyName": {
                    "type": "string",
                    "description": "Target class-reference property name on the Blueprint generated class.",
                },
                "valueClassPath": {
                    "type": "string",
                    "description": (
                        "Class path or Blueprint asset path to assign. For Blueprint assets, the generated "
                        "class will be resolved automatically."
                    ),
                },
                "saveAsset": {
                    "type": "boolean",
                    "default": True,
                    "description": "Save the updated Blueprint asset to disk before responding.",
                },
            },
            "required": ["assetPath", "propertyName", "valueClassPath"],
            "additionalProperties": False,
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "mcpProtocolVersion": {"type": "string"},
                "saved": {"type": "boolean"},
                "success": {"type": "boolean"},
                "message": {"type": "string"},
                "assetPath": {"type": "string"},
                "assetObjectPath": {"type": "string"},
                "packagePath": {"type": "string"},
                "assetName": {"type": "string"},
                "propertyName": {"type": "string"},
                "valueClassPath": {"type": "string"},
                "valueClassName": {"type": "string"},
                "editorReachable": {"type": "boolean"},
            },
            "required": [
                "mcpProtocolVersion",
                "saved",
                "success",
                "message",
                "assetPath",
                "assetObjectPath",
                "packagePath",
                "assetName",
                "propertyName",
                "valueClassPath",
                "valueClassName",
                "editorReachable",
            ],
            "additionalProperties": False,
        },
    }


def build_set_widget_image_texture_tool_definition() -> dict[str, Any]:
    return {
        "name": SET_WIDGET_IMAGE_TEXTURE_TOOL_NAME,
        "title": "Set Widget Blueprint image texture",
        "description": (
            "Assign a texture asset to a named UImage widget inside a Widget Blueprint."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "assetPath": {
                    "type": "string",
                    "description": "Widget Blueprint asset path to update.",
                },
                "widgetName": {
                    "type": "string",
                    "description": "Name of the target UImage widget inside the Widget Blueprint.",
                },
                "textureAssetPath": {
                    "type": "string",
                    "description": "Texture asset path to assign to the image brush.",
                },
                "matchTextureSize": {
                    "type": "boolean",
                    "default": False,
                    "description": "Resize the image widget brush size to match the imported texture.",
                },
                "saveAsset": {
                    "type": "boolean",
                    "default": True,
                    "description": "Save the updated Widget Blueprint asset to disk before responding.",
                },
            },
            "required": ["assetPath", "widgetName", "textureAssetPath"],
            "additionalProperties": False,
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "mcpProtocolVersion": {"type": "string"},
                "saved": {"type": "boolean"},
                "success": {"type": "boolean"},
                "message": {"type": "string"},
                "assetPath": {"type": "string"},
                "assetObjectPath": {"type": "string"},
                "packagePath": {"type": "string"},
                "assetName": {"type": "string"},
                "widgetName": {"type": "string"},
                "textureAssetPath": {"type": "string"},
                "editorReachable": {"type": "boolean"},
            },
            "required": [
                "mcpProtocolVersion",
                "saved",
                "success",
                "message",
                "assetPath",
                "assetObjectPath",
                "packagePath",
                "assetName",
                "widgetName",
                "textureAssetPath",
                "editorReachable",
            ],
            "additionalProperties": False,
        },
    }


def build_set_global_default_game_mode_tool_definition() -> dict[str, Any]:
    return {
        "name": SET_GLOBAL_DEFAULT_GAME_MODE_TOOL_NAME,
        "title": "Set Unreal default game mode",
        "description": "Set the project's global default game mode to the specified GameMode class.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "gameModeClassPath": {
                    "type": "string",
                    "description": (
                        "GameMode class path or GameMode Blueprint asset path. Blueprint assets will resolve "
                        "to their generated class automatically."
                    ),
                },
                "saveConfig": {
                    "type": "boolean",
                    "default": True,
                    "description": "Write the updated default game mode to config before responding.",
                },
            },
            "required": ["gameModeClassPath"],
            "additionalProperties": False,
        },
        "outputSchema": {
            "type": "object",
            "properties": {
                "mcpProtocolVersion": {"type": "string"},
                "saved": {"type": "boolean"},
                "success": {"type": "boolean"},
                "message": {"type": "string"},
                "gameModeClassPath": {"type": "string"},
                "editorReachable": {"type": "boolean"},
            },
            "required": [
                "mcpProtocolVersion",
                "saved",
                "success",
                "message",
                "gameModeClassPath",
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


def build_create_blueprint_asset_tool_success(arguments: dict[str, Any]) -> dict[str, Any]:
    asset_path = arguments.get("assetPath")
    if not isinstance(asset_path, str) or not asset_path.strip():
        raise JsonRpcError(-32602, "ue_create_blueprint_asset.assetPath must be a non-empty string.")

    parent_class_path = arguments.get("parentClassPath")
    if not isinstance(parent_class_path, str) or not parent_class_path.strip():
        raise JsonRpcError(-32602, "ue_create_blueprint_asset.parentClassPath must be a non-empty string.")

    save_asset = arguments.get("saveAsset", True)
    if not isinstance(save_asset, bool):
        raise JsonRpcError(-32602, "ue_create_blueprint_asset.saveAsset must be a boolean.")

    bridge_result = call_ue_bridge(
        "create_blueprint_asset",
        {
            "assetPath": asset_path,
            "parentClassPath": parent_class_path,
            "saveAsset": save_asset,
        },
        timeout_seconds=CREATE_BLUEPRINT_ASSET_TIMEOUT_SECONDS,
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
        "generatedClassPath": str(bridge_result.get("generatedClassPath", "")),
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


def build_create_blueprint_asset_tool_error(
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
        "generatedClassPath": "",
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


def build_import_texture_asset_tool_success(arguments: dict[str, Any]) -> dict[str, Any]:
    source_file_path = arguments.get("sourceFilePath")
    if not isinstance(source_file_path, str) or not source_file_path.strip():
        raise JsonRpcError(-32602, "ue_import_texture_asset.sourceFilePath must be a non-empty string.")

    asset_path = arguments.get("assetPath")
    if not isinstance(asset_path, str) or not asset_path.strip():
        raise JsonRpcError(-32602, "ue_import_texture_asset.assetPath must be a non-empty string.")

    replace_existing = arguments.get("replaceExisting", True)
    if not isinstance(replace_existing, bool):
        raise JsonRpcError(-32602, "ue_import_texture_asset.replaceExisting must be a boolean.")

    save_asset = arguments.get("saveAsset", True)
    if not isinstance(save_asset, bool):
        raise JsonRpcError(-32602, "ue_import_texture_asset.saveAsset must be a boolean.")

    bridge_result = call_ue_bridge(
        "import_texture_asset",
        {
            "sourceFilePath": source_file_path,
            "assetPath": asset_path,
            "replaceExisting": replace_existing,
            "saveAsset": save_asset,
        },
        timeout_seconds=IMPORT_TEXTURE_ASSET_TIMEOUT_SECONDS,
    )

    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "imported": bool(bridge_result.get("imported", False)),
        "saved": bool(bridge_result.get("saved", False)),
        "success": bool(bridge_result.get("success", False)),
        "message": str(bridge_result.get("message", "")),
        "sourceFilePath": str(bridge_result.get("sourceFilePath", source_file_path)),
        "assetPath": str(bridge_result.get("assetPath", "")),
        "assetObjectPath": str(bridge_result.get("assetObjectPath", "")),
        "packagePath": str(bridge_result.get("packagePath", "")),
        "assetName": str(bridge_result.get("assetName", "")),
        "editorReachable": True,
    }

    summary = (
        f"imported={structured_content['imported']} | "
        f"saved={structured_content['saved']} | "
        f"asset={structured_content['assetObjectPath'] or structured_content['assetPath']} | "
        f"{structured_content['message']}"
    )

    return {
        "content": [{"type": "text", "text": summary}],
        "structuredContent": structured_content,
        "isError": not structured_content["success"],
    }


def build_import_texture_asset_tool_error(
    message: str, editor_reachable: bool, source_file_path: str, asset_path: str
) -> dict[str, Any]:
    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "imported": False,
        "saved": False,
        "success": False,
        "message": message,
        "sourceFilePath": source_file_path,
        "assetPath": asset_path,
        "assetObjectPath": "",
        "packagePath": "",
        "assetName": "",
        "editorReachable": editor_reachable,
    }

    return {
        "content": [{"type": "text", "text": message}],
        "structuredContent": structured_content,
        "isError": True,
    }


def build_reorder_widget_child_tool_success(arguments: dict[str, Any]) -> dict[str, Any]:
    asset_path = arguments.get("assetPath")
    if not isinstance(asset_path, str) or not asset_path.strip():
        raise JsonRpcError(-32602, "ue_reorder_widget_child.assetPath must be a non-empty string.")

    widget_name = arguments.get("widgetName")
    if not isinstance(widget_name, str) or not widget_name.strip():
        raise JsonRpcError(-32602, "ue_reorder_widget_child.widgetName must be a non-empty string.")

    desired_index = arguments.get("desiredIndex")
    if not isinstance(desired_index, int):
        raise JsonRpcError(-32602, "ue_reorder_widget_child.desiredIndex must be an integer.")

    save_asset = arguments.get("saveAsset", True)
    if not isinstance(save_asset, bool):
        raise JsonRpcError(-32602, "ue_reorder_widget_child.saveAsset must be a boolean.")

    bridge_result = call_ue_bridge(
        "reorder_widget_child",
        {
            "assetPath": asset_path,
            "widgetName": widget_name,
            "desiredIndex": desired_index,
            "saveAsset": save_asset,
        },
        timeout_seconds=REORDER_WIDGET_CHILD_TIMEOUT_SECONDS,
    )

    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "reordered": bool(bridge_result.get("reordered", False)),
        "saved": bool(bridge_result.get("saved", False)),
        "success": bool(bridge_result.get("success", False)),
        "message": str(bridge_result.get("message", "")),
        "assetPath": str(bridge_result.get("assetPath", "")),
        "assetObjectPath": str(bridge_result.get("assetObjectPath", "")),
        "packagePath": str(bridge_result.get("packagePath", "")),
        "assetName": str(bridge_result.get("assetName", "")),
        "widgetName": str(bridge_result.get("widgetName", widget_name)),
        "parentWidgetName": str(bridge_result.get("parentWidgetName", "")),
        "requestedIndex": int(bridge_result.get("requestedIndex", desired_index)),
        "finalIndex": int(bridge_result.get("finalIndex", -1)),
        "editorReachable": True,
    }

    summary = (
        f"reordered={structured_content['reordered']} | "
        f"saved={structured_content['saved']} | "
        f"asset={structured_content['assetObjectPath'] or structured_content['assetPath']} | "
        f"widget={structured_content['widgetName']} | "
        f"parent={structured_content['parentWidgetName']} | "
        f"index={structured_content['finalIndex']} | "
        f"{structured_content['message']}"
    )

    return {
        "content": [{"type": "text", "text": summary}],
        "structuredContent": structured_content,
        "isError": not structured_content["success"],
    }


def build_reorder_widget_child_tool_error(
    message: str, editor_reachable: bool, asset_path: str, widget_name: str, desired_index: int
) -> dict[str, Any]:
    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "reordered": False,
        "saved": False,
        "success": False,
        "message": message,
        "assetPath": asset_path,
        "assetObjectPath": "",
        "packagePath": "",
        "assetName": "",
        "widgetName": widget_name,
        "parentWidgetName": "",
        "requestedIndex": desired_index,
        "finalIndex": -1,
        "editorReachable": editor_reachable,
    }

    return {
        "content": [{"type": "text", "text": message}],
        "structuredContent": structured_content,
        "isError": True,
    }


def build_remove_widget_tool_success(arguments: dict[str, Any]) -> dict[str, Any]:
    asset_path = arguments.get("assetPath")
    if not isinstance(asset_path, str) or not asset_path.strip():
        raise JsonRpcError(-32602, "ue_remove_widget.assetPath must be a non-empty string.")

    widget_name = arguments.get("widgetName")
    if not isinstance(widget_name, str) or not widget_name.strip():
        raise JsonRpcError(-32602, "ue_remove_widget.widgetName must be a non-empty string.")

    save_asset = arguments.get("saveAsset", True)
    if not isinstance(save_asset, bool):
        raise JsonRpcError(-32602, "ue_remove_widget.saveAsset must be a boolean.")

    bridge_result = call_ue_bridge(
        "remove_widget",
        {
            "assetPath": asset_path,
            "widgetName": widget_name,
            "saveAsset": save_asset,
        },
        timeout_seconds=REMOVE_WIDGET_TIMEOUT_SECONDS,
    )

    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "removed": bool(bridge_result.get("removed", False)),
        "saved": bool(bridge_result.get("saved", False)),
        "success": bool(bridge_result.get("success", False)),
        "message": str(bridge_result.get("message", "")),
        "assetPath": str(bridge_result.get("assetPath", "")),
        "assetObjectPath": str(bridge_result.get("assetObjectPath", "")),
        "packagePath": str(bridge_result.get("packagePath", "")),
        "assetName": str(bridge_result.get("assetName", "")),
        "widgetName": str(bridge_result.get("widgetName", widget_name)),
        "parentWidgetName": str(bridge_result.get("parentWidgetName", "")),
        "editorReachable": True,
    }

    summary = (
        f"removed={structured_content['removed']} | "
        f"saved={structured_content['saved']} | "
        f"asset={structured_content['assetObjectPath'] or structured_content['assetPath']} | "
        f"widget={structured_content['widgetName']} | "
        f"{structured_content['message']}"
    )

    return {
        "content": [{"type": "text", "text": summary}],
        "structuredContent": structured_content,
        "isError": not structured_content["success"],
    }


def build_remove_widget_tool_error(
    message: str, editor_reachable: bool, asset_path: str, widget_name: str
) -> dict[str, Any]:
    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "removed": False,
        "saved": False,
        "success": False,
        "message": message,
        "assetPath": asset_path,
        "assetObjectPath": "",
        "packagePath": "",
        "assetName": "",
        "widgetName": widget_name,
        "parentWidgetName": "",
        "editorReachable": editor_reachable,
    }

    return {
        "content": [{"type": "text", "text": message}],
        "structuredContent": structured_content,
        "isError": True,
    }


def build_set_widget_background_blur_tool_success(arguments: dict[str, Any]) -> dict[str, Any]:
    asset_path = arguments.get("assetPath")
    if not isinstance(asset_path, str) or not asset_path.strip():
        raise JsonRpcError(-32602, "ue_set_widget_background_blur.assetPath must be a non-empty string.")

    widget_name = arguments.get("widgetName")
    if not isinstance(widget_name, str) or not widget_name.strip():
        raise JsonRpcError(-32602, "ue_set_widget_background_blur.widgetName must be a non-empty string.")

    blur_strength = arguments.get("blurStrength", 30.0)
    if not isinstance(blur_strength, (int, float)):
        raise JsonRpcError(-32602, "ue_set_widget_background_blur.blurStrength must be a number.")

    blur_radius = arguments.get("blurRadius")
    if blur_radius is not None and not isinstance(blur_radius, int):
        raise JsonRpcError(-32602, "ue_set_widget_background_blur.blurRadius must be an integer.")

    apply_alpha_to_blur = arguments.get("applyAlphaToBlur", False)
    if not isinstance(apply_alpha_to_blur, bool):
        raise JsonRpcError(-32602, "ue_set_widget_background_blur.applyAlphaToBlur must be a boolean.")

    save_asset = arguments.get("saveAsset", True)
    if not isinstance(save_asset, bool):
        raise JsonRpcError(-32602, "ue_set_widget_background_blur.saveAsset must be a boolean.")

    bridge_arguments: dict[str, Any] = {
        "assetPath": asset_path,
        "widgetName": widget_name,
        "blurStrength": float(blur_strength),
        "applyAlphaToBlur": apply_alpha_to_blur,
        "saveAsset": save_asset,
    }
    if blur_radius is not None:
        bridge_arguments["blurRadius"] = blur_radius

    bridge_result = call_ue_bridge(
        "set_widget_background_blur",
        bridge_arguments,
        timeout_seconds=SET_WIDGET_BACKGROUND_BLUR_TIMEOUT_SECONDS,
    )

    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "converted": bool(bridge_result.get("converted", False)),
        "saved": bool(bridge_result.get("saved", False)),
        "success": bool(bridge_result.get("success", False)),
        "message": str(bridge_result.get("message", "")),
        "assetPath": str(bridge_result.get("assetPath", "")),
        "assetObjectPath": str(bridge_result.get("assetObjectPath", "")),
        "packagePath": str(bridge_result.get("packagePath", "")),
        "assetName": str(bridge_result.get("assetName", "")),
        "widgetName": str(bridge_result.get("widgetName", widget_name)),
        "widgetClassName": str(bridge_result.get("widgetClassName", "")),
        "blurStrength": float(bridge_result.get("blurStrength", float(blur_strength))),
        "blurRadius": int(bridge_result.get("blurRadius", int(blur_radius) if blur_radius is not None else 0)),
        "applyAlphaToBlur": bool(bridge_result.get("applyAlphaToBlur", apply_alpha_to_blur)),
        "overrideAutoRadiusCalculation": bool(bridge_result.get("overrideAutoRadiusCalculation", blur_radius is not None)),
        "editorReachable": True,
    }

    summary = (
        f"converted={structured_content['converted']} | "
        f"saved={structured_content['saved']} | "
        f"asset={structured_content['assetObjectPath'] or structured_content['assetPath']} | "
        f"widget={structured_content['widgetName']} | "
        f"class={structured_content['widgetClassName']} | "
        f"blur={structured_content['blurStrength']} | "
        f"{structured_content['message']}"
    )

    return {
        "content": [{"type": "text", "text": summary}],
        "structuredContent": structured_content,
        "isError": not structured_content["success"],
    }


def build_set_widget_background_blur_tool_error(
    message: str, editor_reachable: bool, asset_path: str, widget_name: str
) -> dict[str, Any]:
    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "converted": False,
        "saved": False,
        "success": False,
        "message": message,
        "assetPath": asset_path,
        "assetObjectPath": "",
        "packagePath": "",
        "assetName": "",
        "widgetName": widget_name,
        "widgetClassName": "",
        "blurStrength": 0.0,
        "blurRadius": 0,
        "applyAlphaToBlur": False,
        "overrideAutoRadiusCalculation": False,
        "editorReachable": editor_reachable,
    }

    return {
        "content": [{"type": "text", "text": message}],
        "structuredContent": structured_content,
        "isError": True,
    }


def build_set_widget_corner_radius_tool_success(arguments: dict[str, Any]) -> dict[str, Any]:
    asset_path = arguments.get("assetPath")
    if not isinstance(asset_path, str) or not asset_path.strip():
        raise JsonRpcError(-32602, "ue_set_widget_corner_radius.assetPath must be a non-empty string.")

    widget_name = arguments.get("widgetName")
    if not isinstance(widget_name, str) or not widget_name.strip():
        raise JsonRpcError(-32602, "ue_set_widget_corner_radius.widgetName must be a non-empty string.")

    radius = arguments.get("radius")
    if not isinstance(radius, (int, float)):
        raise JsonRpcError(-32602, "ue_set_widget_corner_radius.radius must be a number.")

    save_asset = arguments.get("saveAsset", True)
    if not isinstance(save_asset, bool):
        raise JsonRpcError(-32602, "ue_set_widget_corner_radius.saveAsset must be a boolean.")

    bridge_result = call_ue_bridge(
        "set_widget_corner_radius",
        {
            "assetPath": asset_path,
            "widgetName": widget_name,
            "radius": float(radius),
            "saveAsset": save_asset,
        },
        timeout_seconds=SET_WIDGET_CORNER_RADIUS_TIMEOUT_SECONDS,
    )

    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "saved": bool(bridge_result.get("saved", False)),
        "success": bool(bridge_result.get("success", False)),
        "message": str(bridge_result.get("message", "")),
        "assetPath": str(bridge_result.get("assetPath", "")),
        "assetObjectPath": str(bridge_result.get("assetObjectPath", "")),
        "packagePath": str(bridge_result.get("packagePath", "")),
        "assetName": str(bridge_result.get("assetName", "")),
        "widgetName": str(bridge_result.get("widgetName", widget_name)),
        "widgetClassName": str(bridge_result.get("widgetClassName", "")),
        "radius": float(bridge_result.get("radius", float(radius))),
        "editorReachable": True,
    }

    summary = (
        f"saved={structured_content['saved']} | "
        f"asset={structured_content['assetObjectPath'] or structured_content['assetPath']} | "
        f"widget={structured_content['widgetName']} | "
        f"radius={structured_content['radius']} | "
        f"{structured_content['message']}"
    )

    return {
        "content": [{"type": "text", "text": summary}],
        "structuredContent": structured_content,
        "isError": not structured_content["success"],
    }


def build_set_widget_corner_radius_tool_error(
    message: str, editor_reachable: bool, asset_path: str, widget_name: str
) -> dict[str, Any]:
    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "saved": False,
        "success": False,
        "message": message,
        "assetPath": asset_path,
        "assetObjectPath": "",
        "packagePath": "",
        "assetName": "",
        "widgetName": widget_name,
        "widgetClassName": "",
        "radius": 0.0,
        "editorReachable": editor_reachable,
    }

    return {
        "content": [{"type": "text", "text": message}],
        "structuredContent": structured_content,
        "isError": True,
    }


def build_set_widget_panel_color_tool_success(arguments: dict[str, Any]) -> dict[str, Any]:
    asset_path = arguments.get("assetPath")
    if not isinstance(asset_path, str) or not asset_path.strip():
        raise JsonRpcError(-32602, "ue_set_widget_panel_color.assetPath must be a non-empty string.")

    widget_name = arguments.get("widgetName")
    if not isinstance(widget_name, str) or not widget_name.strip():
        raise JsonRpcError(-32602, "ue_set_widget_panel_color.widgetName must be a non-empty string.")

    red = arguments.get("red")
    green = arguments.get("green")
    blue = arguments.get("blue")
    alpha = arguments.get("alpha")
    if not isinstance(red, (int, float)):
        raise JsonRpcError(-32602, "ue_set_widget_panel_color.red must be a number.")
    if not isinstance(green, (int, float)):
        raise JsonRpcError(-32602, "ue_set_widget_panel_color.green must be a number.")
    if not isinstance(blue, (int, float)):
        raise JsonRpcError(-32602, "ue_set_widget_panel_color.blue must be a number.")
    if not isinstance(alpha, (int, float)):
        raise JsonRpcError(-32602, "ue_set_widget_panel_color.alpha must be a number.")

    save_asset = arguments.get("saveAsset", True)
    if not isinstance(save_asset, bool):
        raise JsonRpcError(-32602, "ue_set_widget_panel_color.saveAsset must be a boolean.")

    bridge_result = call_ue_bridge(
        "set_widget_panel_color",
        {
            "assetPath": asset_path,
            "widgetName": widget_name,
            "red": float(red),
            "green": float(green),
            "blue": float(blue),
            "alpha": float(alpha),
            "saveAsset": save_asset,
        },
        timeout_seconds=SET_WIDGET_PANEL_COLOR_TIMEOUT_SECONDS,
    )

    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "saved": bool(bridge_result.get("saved", False)),
        "success": bool(bridge_result.get("success", False)),
        "message": str(bridge_result.get("message", "")),
        "assetPath": str(bridge_result.get("assetPath", "")),
        "assetObjectPath": str(bridge_result.get("assetObjectPath", "")),
        "packagePath": str(bridge_result.get("packagePath", "")),
        "assetName": str(bridge_result.get("assetName", "")),
        "widgetName": str(bridge_result.get("widgetName", widget_name)),
        "widgetClassName": str(bridge_result.get("widgetClassName", "")),
        "red": float(bridge_result.get("red", 0.0)),
        "green": float(bridge_result.get("green", 0.0)),
        "blue": float(bridge_result.get("blue", 0.0)),
        "alpha": float(bridge_result.get("alpha", 0.0)),
        "editorReachable": True,
    }

    summary = (
        f"saved={structured_content['saved']} | "
        f"asset={structured_content['assetObjectPath'] or structured_content['assetPath']} | "
        f"widget={structured_content['widgetName']} | "
        f"class={structured_content['widgetClassName']} | "
        f"rgba=({structured_content['red']:.3f}, {structured_content['green']:.3f}, "
        f"{structured_content['blue']:.3f}, {structured_content['alpha']:.3f}) | "
        f"{structured_content['message']}"
    )

    return {
        "content": [{"type": "text", "text": summary}],
        "structuredContent": structured_content,
        "isError": not structured_content["success"],
    }


def build_set_widget_panel_color_tool_error(
    message: str, editor_reachable: bool, asset_path: str, widget_name: str
) -> dict[str, Any]:
    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "saved": False,
        "success": False,
        "message": message,
        "assetPath": asset_path,
        "assetObjectPath": "",
        "packagePath": "",
        "assetName": "",
        "widgetName": widget_name,
        "widgetClassName": "",
        "red": 0.0,
        "green": 0.0,
        "blue": 0.0,
        "alpha": 0.0,
        "editorReachable": editor_reachable,
    }

    return {
        "content": [{"type": "text", "text": message}],
        "structuredContent": structured_content,
        "isError": True,
    }


def build_set_popup_open_elastic_scale_tool_success(arguments: dict[str, Any]) -> dict[str, Any]:
    asset_path = arguments.get("assetPath")
    if not isinstance(asset_path, str) or not asset_path.strip():
        raise JsonRpcError(-32602, "ue_set_popup_open_elastic_scale.assetPath must be a non-empty string.")

    enabled = arguments.get("enabled", True)
    if not isinstance(enabled, bool):
        raise JsonRpcError(-32602, "ue_set_popup_open_elastic_scale.enabled must be a boolean.")

    widget_name = arguments.get("widgetName", "PopupCard")
    if not isinstance(widget_name, str) or not widget_name.strip():
        raise JsonRpcError(-32602, "ue_set_popup_open_elastic_scale.widgetName must be a non-empty string.")

    duration = arguments.get("duration", 0.45)
    if not isinstance(duration, (int, float)):
        raise JsonRpcError(-32602, "ue_set_popup_open_elastic_scale.duration must be a number.")

    start_scale = arguments.get("startScale", 0.82)
    if not isinstance(start_scale, (int, float)):
        raise JsonRpcError(-32602, "ue_set_popup_open_elastic_scale.startScale must be a number.")

    oscillation_count = arguments.get("oscillationCount", 2.0)
    if not isinstance(oscillation_count, (int, float)):
        raise JsonRpcError(-32602, "ue_set_popup_open_elastic_scale.oscillationCount must be a number.")

    pivot_x = arguments.get("pivotX", 0.5)
    if not isinstance(pivot_x, (int, float)):
        raise JsonRpcError(-32602, "ue_set_popup_open_elastic_scale.pivotX must be a number.")

    pivot_y = arguments.get("pivotY", 0.5)
    if not isinstance(pivot_y, (int, float)):
        raise JsonRpcError(-32602, "ue_set_popup_open_elastic_scale.pivotY must be a number.")

    save_asset = arguments.get("saveAsset", True)
    if not isinstance(save_asset, bool):
        raise JsonRpcError(-32602, "ue_set_popup_open_elastic_scale.saveAsset must be a boolean.")

    bridge_result = call_ue_bridge(
        "set_popup_open_elastic_scale",
        {
            "assetPath": asset_path,
            "enabled": enabled,
            "widgetName": widget_name,
            "duration": float(duration),
            "startScale": float(start_scale),
            "oscillationCount": float(oscillation_count),
            "pivotX": float(pivot_x),
            "pivotY": float(pivot_y),
            "saveAsset": save_asset,
        },
        timeout_seconds=SET_POPUP_OPEN_ELASTIC_SCALE_TIMEOUT_SECONDS,
    )

    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "saved": bool(bridge_result.get("saved", False)),
        "success": bool(bridge_result.get("success", False)),
        "enabled": bool(bridge_result.get("enabled", enabled)),
        "message": str(bridge_result.get("message", "")),
        "assetPath": str(bridge_result.get("assetPath", "")),
        "assetObjectPath": str(bridge_result.get("assetObjectPath", "")),
        "packagePath": str(bridge_result.get("packagePath", "")),
        "assetName": str(bridge_result.get("assetName", "")),
        "widgetName": str(bridge_result.get("widgetName", widget_name)),
        "duration": float(bridge_result.get("duration", 0.0)),
        "startScale": float(bridge_result.get("startScale", 0.0)),
        "oscillationCount": float(bridge_result.get("oscillationCount", 0.0)),
        "pivotX": float(bridge_result.get("pivotX", 0.0)),
        "pivotY": float(bridge_result.get("pivotY", 0.0)),
        "editorReachable": True,
    }

    summary = (
        f"saved={structured_content['saved']} | "
        f"asset={structured_content['assetObjectPath'] or structured_content['assetPath']} | "
        f"enabled={structured_content['enabled']} | "
        f"widget={structured_content['widgetName']} | "
        f"duration={structured_content['duration']:.3f}s | "
        f"startScale={structured_content['startScale']:.3f} | "
        f"oscillations={structured_content['oscillationCount']:.3f} | "
        f"pivot=({structured_content['pivotX']:.3f}, {structured_content['pivotY']:.3f}) | "
        f"{structured_content['message']}"
    )

    return {
        "content": [{"type": "text", "text": summary}],
        "structuredContent": structured_content,
        "isError": not structured_content["success"],
    }


def build_set_popup_open_elastic_scale_tool_error(
    message: str, editor_reachable: bool, asset_path: str, widget_name: str
) -> dict[str, Any]:
    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "saved": False,
        "success": False,
        "enabled": False,
        "message": message,
        "assetPath": asset_path,
        "assetObjectPath": "",
        "packagePath": "",
        "assetName": "",
        "widgetName": widget_name,
        "duration": 0.0,
        "startScale": 0.0,
        "oscillationCount": 0.0,
        "pivotX": 0.0,
        "pivotY": 0.0,
        "editorReachable": editor_reachable,
    }

    return {
        "content": [{"type": "text", "text": message}],
        "structuredContent": structured_content,
        "isError": True,
    }


def build_scaffold_widget_blueprint_tool_success(arguments: dict[str, Any]) -> dict[str, Any]:
    asset_path = arguments.get("assetPath")
    if not isinstance(asset_path, str) or not asset_path.strip():
        raise JsonRpcError(-32602, "ue_scaffold_widget_blueprint.assetPath must be a non-empty string.")

    scaffold_type = arguments.get("scaffoldType")
    if not isinstance(scaffold_type, str) or scaffold_type not in {"popup", "bottom_button_bar"}:
        raise JsonRpcError(
            -32602,
            "ue_scaffold_widget_blueprint.scaffoldType must be one of: popup, bottom_button_bar.",
        )

    save_asset = arguments.get("saveAsset", True)
    if not isinstance(save_asset, bool):
        raise JsonRpcError(-32602, "ue_scaffold_widget_blueprint.saveAsset must be a boolean.")

    bridge_result = call_ue_bridge(
        "scaffold_widget_blueprint",
        {
            "assetPath": asset_path,
            "scaffoldType": scaffold_type,
            "saveAsset": save_asset,
        },
        timeout_seconds=SCAFFOLD_WIDGET_BLUEPRINT_TIMEOUT_SECONDS,
    )

    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "saved": bool(bridge_result.get("saved", False)),
        "success": bool(bridge_result.get("success", False)),
        "message": str(bridge_result.get("message", "")),
        "assetPath": str(bridge_result.get("assetPath", "")),
        "assetObjectPath": str(bridge_result.get("assetObjectPath", "")),
        "packagePath": str(bridge_result.get("packagePath", "")),
        "assetName": str(bridge_result.get("assetName", "")),
        "scaffoldType": str(bridge_result.get("scaffoldType", scaffold_type)),
        "editorReachable": True,
    }

    summary = (
        f"saved={structured_content['saved']} | "
        f"asset={structured_content['assetObjectPath'] or structured_content['assetPath']} | "
        f"scaffold={structured_content['scaffoldType']} | "
        f"{structured_content['message']}"
    )

    return {
        "content": [{"type": "text", "text": summary}],
        "structuredContent": structured_content,
        "isError": not structured_content["success"],
    }


def build_scaffold_widget_blueprint_tool_error(
    message: str, editor_reachable: bool, asset_path: str, scaffold_type: str
) -> dict[str, Any]:
    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "saved": False,
        "success": False,
        "message": message,
        "assetPath": asset_path,
        "assetObjectPath": "",
        "packagePath": "",
        "assetName": "",
        "scaffoldType": scaffold_type,
        "editorReachable": editor_reachable,
    }

    return {
        "content": [{"type": "text", "text": message}],
        "structuredContent": structured_content,
        "isError": True,
    }


def build_set_blueprint_class_property_tool_success(arguments: dict[str, Any]) -> dict[str, Any]:
    asset_path = arguments.get("assetPath")
    if not isinstance(asset_path, str) or not asset_path.strip():
        raise JsonRpcError(-32602, "ue_set_blueprint_class_property.assetPath must be a non-empty string.")

    property_name = arguments.get("propertyName")
    if not isinstance(property_name, str) or not property_name.strip():
        raise JsonRpcError(-32602, "ue_set_blueprint_class_property.propertyName must be a non-empty string.")

    value_class_path = arguments.get("valueClassPath")
    if not isinstance(value_class_path, str) or not value_class_path.strip():
        raise JsonRpcError(-32602, "ue_set_blueprint_class_property.valueClassPath must be a non-empty string.")

    save_asset = arguments.get("saveAsset", True)
    if not isinstance(save_asset, bool):
        raise JsonRpcError(-32602, "ue_set_blueprint_class_property.saveAsset must be a boolean.")

    bridge_result = call_ue_bridge(
        "set_blueprint_class_property",
        {
            "assetPath": asset_path,
            "propertyName": property_name,
            "valueClassPath": value_class_path,
            "saveAsset": save_asset,
        },
        timeout_seconds=SET_BLUEPRINT_CLASS_PROPERTY_TIMEOUT_SECONDS,
    )

    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "saved": bool(bridge_result.get("saved", False)),
        "success": bool(bridge_result.get("success", False)),
        "message": str(bridge_result.get("message", "")),
        "assetPath": str(bridge_result.get("assetPath", "")),
        "assetObjectPath": str(bridge_result.get("assetObjectPath", "")),
        "packagePath": str(bridge_result.get("packagePath", "")),
        "assetName": str(bridge_result.get("assetName", "")),
        "propertyName": str(bridge_result.get("propertyName", property_name)),
        "valueClassPath": str(bridge_result.get("valueClassPath", "")),
        "valueClassName": str(bridge_result.get("valueClassName", "")),
        "editorReachable": True,
    }

    summary = (
        f"saved={structured_content['saved']} | "
        f"asset={structured_content['assetObjectPath'] or structured_content['assetPath']} | "
        f"property={structured_content['propertyName']} | "
        f"value={structured_content['valueClassPath']} | "
        f"{structured_content['message']}"
    )

    return {
        "content": [{"type": "text", "text": summary}],
        "structuredContent": structured_content,
        "isError": not structured_content["success"],
    }


def build_set_blueprint_class_property_tool_error(
    message: str, editor_reachable: bool, asset_path: str, property_name: str, value_class_path: str
) -> dict[str, Any]:
    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "saved": False,
        "success": False,
        "message": message,
        "assetPath": asset_path,
        "assetObjectPath": "",
        "packagePath": "",
        "assetName": "",
        "propertyName": property_name,
        "valueClassPath": value_class_path,
        "valueClassName": "",
        "editorReachable": editor_reachable,
    }

    return {
        "content": [{"type": "text", "text": message}],
        "structuredContent": structured_content,
        "isError": True,
    }


def build_set_widget_image_texture_tool_success(arguments: dict[str, Any]) -> dict[str, Any]:
    asset_path = arguments.get("assetPath")
    if not isinstance(asset_path, str) or not asset_path.strip():
        raise JsonRpcError(-32602, "ue_set_widget_image_texture.assetPath must be a non-empty string.")

    widget_name = arguments.get("widgetName")
    if not isinstance(widget_name, str) or not widget_name.strip():
        raise JsonRpcError(-32602, "ue_set_widget_image_texture.widgetName must be a non-empty string.")

    texture_asset_path = arguments.get("textureAssetPath")
    if not isinstance(texture_asset_path, str) or not texture_asset_path.strip():
        raise JsonRpcError(-32602, "ue_set_widget_image_texture.textureAssetPath must be a non-empty string.")

    match_texture_size = arguments.get("matchTextureSize", False)
    if not isinstance(match_texture_size, bool):
        raise JsonRpcError(-32602, "ue_set_widget_image_texture.matchTextureSize must be a boolean.")

    save_asset = arguments.get("saveAsset", True)
    if not isinstance(save_asset, bool):
        raise JsonRpcError(-32602, "ue_set_widget_image_texture.saveAsset must be a boolean.")

    bridge_result = call_ue_bridge(
        "set_widget_image_texture",
        {
            "assetPath": asset_path,
            "widgetName": widget_name,
            "textureAssetPath": texture_asset_path,
            "matchTextureSize": match_texture_size,
            "saveAsset": save_asset,
        },
        timeout_seconds=SET_WIDGET_IMAGE_TEXTURE_TIMEOUT_SECONDS,
    )

    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "saved": bool(bridge_result.get("saved", False)),
        "success": bool(bridge_result.get("success", False)),
        "message": str(bridge_result.get("message", "")),
        "assetPath": str(bridge_result.get("assetPath", "")),
        "assetObjectPath": str(bridge_result.get("assetObjectPath", "")),
        "packagePath": str(bridge_result.get("packagePath", "")),
        "assetName": str(bridge_result.get("assetName", "")),
        "widgetName": str(bridge_result.get("widgetName", widget_name)),
        "textureAssetPath": str(bridge_result.get("textureAssetPath", "")),
        "editorReachable": True,
    }

    summary = (
        f"saved={structured_content['saved']} | "
        f"asset={structured_content['assetObjectPath'] or structured_content['assetPath']} | "
        f"widget={structured_content['widgetName']} | "
        f"texture={structured_content['textureAssetPath']} | "
        f"{structured_content['message']}"
    )

    return {
        "content": [{"type": "text", "text": summary}],
        "structuredContent": structured_content,
        "isError": not structured_content["success"],
    }


def build_set_widget_image_texture_tool_error(
    message: str, editor_reachable: bool, asset_path: str, widget_name: str, texture_asset_path: str
) -> dict[str, Any]:
    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "saved": False,
        "success": False,
        "message": message,
        "assetPath": asset_path,
        "assetObjectPath": "",
        "packagePath": "",
        "assetName": "",
        "widgetName": widget_name,
        "textureAssetPath": texture_asset_path,
        "editorReachable": editor_reachable,
    }

    return {
        "content": [{"type": "text", "text": message}],
        "structuredContent": structured_content,
        "isError": True,
    }


def build_set_global_default_game_mode_tool_success(arguments: dict[str, Any]) -> dict[str, Any]:
    game_mode_class_path = arguments.get("gameModeClassPath")
    if not isinstance(game_mode_class_path, str) or not game_mode_class_path.strip():
        raise JsonRpcError(-32602, "ue_set_global_default_game_mode.gameModeClassPath must be a non-empty string.")

    save_config = arguments.get("saveConfig", True)
    if not isinstance(save_config, bool):
        raise JsonRpcError(-32602, "ue_set_global_default_game_mode.saveConfig must be a boolean.")

    bridge_result = call_ue_bridge(
        "set_global_default_game_mode",
        {
            "gameModeClassPath": game_mode_class_path,
            "saveConfig": save_config,
        },
        timeout_seconds=SET_GLOBAL_DEFAULT_GAME_MODE_TIMEOUT_SECONDS,
    )

    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "saved": bool(bridge_result.get("saved", False)),
        "success": bool(bridge_result.get("success", False)),
        "message": str(bridge_result.get("message", "")),
        "gameModeClassPath": str(bridge_result.get("gameModeClassPath", "")),
        "editorReachable": True,
    }

    summary = (
        f"saved={structured_content['saved']} | "
        f"gameMode={structured_content['gameModeClassPath']} | "
        f"{structured_content['message']}"
    )

    return {
        "content": [{"type": "text", "text": summary}],
        "structuredContent": structured_content,
        "isError": not structured_content["success"],
    }


def build_set_global_default_game_mode_tool_error(
    message: str, editor_reachable: bool, game_mode_class_path: str
) -> dict[str, Any]:
    structured_content = {
        "mcpProtocolVersion": MCP_PROTOCOL_VERSION,
        "saved": False,
        "success": False,
        "message": message,
        "gameModeClassPath": game_mode_class_path,
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
                "ue_create_blueprint_asset to generate a non-UMG Blueprint asset from a parent class, "
                "ue_create_widget_blueprint to generate a Widget Blueprint asset from a parent class, "
                "ue_import_texture_asset to import a disk image into the project, "
                "ue_reorder_widget_child to reorder a widget inside its current parent in a Widget Blueprint, "
                "ue_remove_widget to remove a widget from a Widget Blueprint, "
                "ue_set_widget_background_blur to convert/configure a widget as a BackgroundBlur panel, "
                "ue_set_widget_corner_radius to set rounded corners on supported panel widgets, "
                "ue_set_widget_panel_color to set RGBA color on supported panel widgets, "
                "ue_set_popup_open_elastic_scale to configure popup open elastic scale animation defaults, "
                "ue_scaffold_widget_blueprint to populate an existing Widget Blueprint with a predefined tree, "
                "ue_set_blueprint_class_property to wire Blueprint class-reference defaults, "
                "ue_set_widget_image_texture to assign a texture to a UImage in a Widget Blueprint, "
                "or ue_set_global_default_game_mode to update the project's default GameMode."
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
                build_create_blueprint_asset_tool_definition(),
                build_create_widget_blueprint_tool_definition(),
                build_import_texture_asset_tool_definition(),
                build_reorder_widget_child_tool_definition(),
                build_remove_widget_tool_definition(),
                build_set_widget_background_blur_tool_definition(),
                build_set_widget_corner_radius_tool_definition(),
                build_set_widget_panel_color_tool_definition(),
                build_set_popup_open_elastic_scale_tool_definition(),
                build_scaffold_widget_blueprint_tool_definition(),
                build_set_blueprint_class_property_tool_definition(),
                build_set_widget_image_texture_tool_definition(),
                build_set_global_default_game_mode_tool_definition(),
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

    if tool_name == CREATE_BLUEPRINT_ASSET_TOOL_NAME:
        asset_path = tool_arguments.get("assetPath", "")
        parent_class_path = tool_arguments.get("parentClassPath", "")
        if asset_path is not None and not isinstance(asset_path, str):
            raise JsonRpcError(-32602, "ue_create_blueprint_asset.assetPath must be a string.")
        if parent_class_path is not None and not isinstance(parent_class_path, str):
            raise JsonRpcError(-32602, "ue_create_blueprint_asset.parentClassPath must be a string.")
        try:
            result = build_create_blueprint_asset_tool_success(tool_arguments)
        except UeBridgeError as exc:
            result = build_create_blueprint_asset_tool_error(
                str(exc),
                exc.editor_reachable,
                str(asset_path or ""),
                str(parent_class_path or ""),
            )
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

    if tool_name == IMPORT_TEXTURE_ASSET_TOOL_NAME:
        source_file_path = tool_arguments.get("sourceFilePath", "")
        asset_path = tool_arguments.get("assetPath", "")
        if source_file_path is not None and not isinstance(source_file_path, str):
            raise JsonRpcError(-32602, "ue_import_texture_asset.sourceFilePath must be a string.")
        if asset_path is not None and not isinstance(asset_path, str):
            raise JsonRpcError(-32602, "ue_import_texture_asset.assetPath must be a string.")
        try:
            result = build_import_texture_asset_tool_success(tool_arguments)
        except UeBridgeError as exc:
            result = build_import_texture_asset_tool_error(
                str(exc),
                exc.editor_reachable,
                str(source_file_path or ""),
                str(asset_path or ""),
            )
        return make_response(message_id, result)

    if tool_name == REORDER_WIDGET_CHILD_TOOL_NAME:
        asset_path = tool_arguments.get("assetPath", "")
        widget_name = tool_arguments.get("widgetName", "")
        desired_index = tool_arguments.get("desiredIndex", -1)
        if asset_path is not None and not isinstance(asset_path, str):
            raise JsonRpcError(-32602, "ue_reorder_widget_child.assetPath must be a string.")
        if widget_name is not None and not isinstance(widget_name, str):
            raise JsonRpcError(-32602, "ue_reorder_widget_child.widgetName must be a string.")
        if desired_index is not None and not isinstance(desired_index, int):
            raise JsonRpcError(-32602, "ue_reorder_widget_child.desiredIndex must be an integer.")
        try:
            result = build_reorder_widget_child_tool_success(tool_arguments)
        except UeBridgeError as exc:
            result = build_reorder_widget_child_tool_error(
                str(exc),
                exc.editor_reachable,
                str(asset_path or ""),
                str(widget_name or ""),
                int(desired_index if isinstance(desired_index, int) else -1),
            )
        return make_response(message_id, result)

    if tool_name == REMOVE_WIDGET_TOOL_NAME:
        asset_path = tool_arguments.get("assetPath", "")
        widget_name = tool_arguments.get("widgetName", "")
        if asset_path is not None and not isinstance(asset_path, str):
            raise JsonRpcError(-32602, "ue_remove_widget.assetPath must be a string.")
        if widget_name is not None and not isinstance(widget_name, str):
            raise JsonRpcError(-32602, "ue_remove_widget.widgetName must be a string.")
        try:
            result = build_remove_widget_tool_success(tool_arguments)
        except UeBridgeError as exc:
            result = build_remove_widget_tool_error(
                str(exc),
                exc.editor_reachable,
                str(asset_path or ""),
                str(widget_name or ""),
            )
        return make_response(message_id, result)

    if tool_name == SET_WIDGET_BACKGROUND_BLUR_TOOL_NAME:
        asset_path = tool_arguments.get("assetPath", "")
        widget_name = tool_arguments.get("widgetName", "")
        blur_strength = tool_arguments.get("blurStrength", 30.0)
        blur_radius = tool_arguments.get("blurRadius")
        if asset_path is not None and not isinstance(asset_path, str):
            raise JsonRpcError(-32602, "ue_set_widget_background_blur.assetPath must be a string.")
        if widget_name is not None and not isinstance(widget_name, str):
            raise JsonRpcError(-32602, "ue_set_widget_background_blur.widgetName must be a string.")
        if blur_strength is not None and not isinstance(blur_strength, (int, float)):
            raise JsonRpcError(-32602, "ue_set_widget_background_blur.blurStrength must be a number.")
        if blur_radius is not None and not isinstance(blur_radius, int):
            raise JsonRpcError(-32602, "ue_set_widget_background_blur.blurRadius must be an integer.")
        try:
            result = build_set_widget_background_blur_tool_success(tool_arguments)
        except UeBridgeError as exc:
            result = build_set_widget_background_blur_tool_error(
                str(exc),
                exc.editor_reachable,
                str(asset_path or ""),
                str(widget_name or ""),
            )
        return make_response(message_id, result)

    if tool_name == SET_WIDGET_CORNER_RADIUS_TOOL_NAME:
        asset_path = tool_arguments.get("assetPath", "")
        widget_name = tool_arguments.get("widgetName", "")
        radius = tool_arguments.get("radius")
        if asset_path is not None and not isinstance(asset_path, str):
            raise JsonRpcError(-32602, "ue_set_widget_corner_radius.assetPath must be a string.")
        if widget_name is not None and not isinstance(widget_name, str):
            raise JsonRpcError(-32602, "ue_set_widget_corner_radius.widgetName must be a string.")
        if radius is not None and not isinstance(radius, (int, float)):
            raise JsonRpcError(-32602, "ue_set_widget_corner_radius.radius must be a number.")
        try:
            result = build_set_widget_corner_radius_tool_success(tool_arguments)
        except UeBridgeError as exc:
            result = build_set_widget_corner_radius_tool_error(
                str(exc),
                exc.editor_reachable,
                str(asset_path or ""),
                str(widget_name or ""),
            )
        return make_response(message_id, result)

    if tool_name == SCAFFOLD_WIDGET_BLUEPRINT_TOOL_NAME:
        asset_path = tool_arguments.get("assetPath", "")
        scaffold_type = tool_arguments.get("scaffoldType", "")
        if asset_path is not None and not isinstance(asset_path, str):
            raise JsonRpcError(-32602, "ue_scaffold_widget_blueprint.assetPath must be a string.")
        if scaffold_type is not None and not isinstance(scaffold_type, str):
            raise JsonRpcError(-32602, "ue_scaffold_widget_blueprint.scaffoldType must be a string.")
        try:
            result = build_scaffold_widget_blueprint_tool_success(tool_arguments)
        except UeBridgeError as exc:
            result = build_scaffold_widget_blueprint_tool_error(
                str(exc),
                exc.editor_reachable,
                str(asset_path or ""),
                str(scaffold_type or ""),
            )
        return make_response(message_id, result)

    if tool_name == SET_BLUEPRINT_CLASS_PROPERTY_TOOL_NAME:
        asset_path = tool_arguments.get("assetPath", "")
        property_name = tool_arguments.get("propertyName", "")
        value_class_path = tool_arguments.get("valueClassPath", "")
        if asset_path is not None and not isinstance(asset_path, str):
            raise JsonRpcError(-32602, "ue_set_blueprint_class_property.assetPath must be a string.")
        if property_name is not None and not isinstance(property_name, str):
            raise JsonRpcError(-32602, "ue_set_blueprint_class_property.propertyName must be a string.")
        if value_class_path is not None and not isinstance(value_class_path, str):
            raise JsonRpcError(-32602, "ue_set_blueprint_class_property.valueClassPath must be a string.")
        try:
            result = build_set_blueprint_class_property_tool_success(tool_arguments)
        except UeBridgeError as exc:
            result = build_set_blueprint_class_property_tool_error(
                str(exc),
                exc.editor_reachable,
                str(asset_path or ""),
                str(property_name or ""),
                str(value_class_path or ""),
            )
        return make_response(message_id, result)

    if tool_name == SET_WIDGET_PANEL_COLOR_TOOL_NAME:
        asset_path = tool_arguments.get("assetPath", "")
        widget_name = tool_arguments.get("widgetName", "")
        red = tool_arguments.get("red")
        green = tool_arguments.get("green")
        blue = tool_arguments.get("blue")
        alpha = tool_arguments.get("alpha")
        if asset_path is not None and not isinstance(asset_path, str):
            raise JsonRpcError(-32602, "ue_set_widget_panel_color.assetPath must be a string.")
        if widget_name is not None and not isinstance(widget_name, str):
            raise JsonRpcError(-32602, "ue_set_widget_panel_color.widgetName must be a string.")
        if red is not None and not isinstance(red, (int, float)):
            raise JsonRpcError(-32602, "ue_set_widget_panel_color.red must be a number.")
        if green is not None and not isinstance(green, (int, float)):
            raise JsonRpcError(-32602, "ue_set_widget_panel_color.green must be a number.")
        if blue is not None and not isinstance(blue, (int, float)):
            raise JsonRpcError(-32602, "ue_set_widget_panel_color.blue must be a number.")
        if alpha is not None and not isinstance(alpha, (int, float)):
            raise JsonRpcError(-32602, "ue_set_widget_panel_color.alpha must be a number.")
        try:
            result = build_set_widget_panel_color_tool_success(tool_arguments)
        except UeBridgeError as exc:
            result = build_set_widget_panel_color_tool_error(
                str(exc),
                exc.editor_reachable,
                str(asset_path or ""),
                str(widget_name or ""),
            )
        return make_response(message_id, result)

    if tool_name == SET_POPUP_OPEN_ELASTIC_SCALE_TOOL_NAME:
        asset_path = tool_arguments.get("assetPath", "")
        widget_name = tool_arguments.get("widgetName", "PopupCard")
        duration = tool_arguments.get("duration", 0.45)
        start_scale = tool_arguments.get("startScale", 0.82)
        oscillation_count = tool_arguments.get("oscillationCount", 2.0)
        pivot_x = tool_arguments.get("pivotX", 0.5)
        pivot_y = tool_arguments.get("pivotY", 0.5)
        if asset_path is not None and not isinstance(asset_path, str):
            raise JsonRpcError(-32602, "ue_set_popup_open_elastic_scale.assetPath must be a string.")
        if widget_name is not None and not isinstance(widget_name, str):
            raise JsonRpcError(-32602, "ue_set_popup_open_elastic_scale.widgetName must be a string.")
        if duration is not None and not isinstance(duration, (int, float)):
            raise JsonRpcError(-32602, "ue_set_popup_open_elastic_scale.duration must be a number.")
        if start_scale is not None and not isinstance(start_scale, (int, float)):
            raise JsonRpcError(-32602, "ue_set_popup_open_elastic_scale.startScale must be a number.")
        if oscillation_count is not None and not isinstance(oscillation_count, (int, float)):
            raise JsonRpcError(-32602, "ue_set_popup_open_elastic_scale.oscillationCount must be a number.")
        if pivot_x is not None and not isinstance(pivot_x, (int, float)):
            raise JsonRpcError(-32602, "ue_set_popup_open_elastic_scale.pivotX must be a number.")
        if pivot_y is not None and not isinstance(pivot_y, (int, float)):
            raise JsonRpcError(-32602, "ue_set_popup_open_elastic_scale.pivotY must be a number.")
        try:
            result = build_set_popup_open_elastic_scale_tool_success(tool_arguments)
        except UeBridgeError as exc:
            result = build_set_popup_open_elastic_scale_tool_error(
                str(exc),
                exc.editor_reachable,
                str(asset_path or ""),
                str(widget_name or ""),
            )
        return make_response(message_id, result)

    if tool_name == SET_WIDGET_IMAGE_TEXTURE_TOOL_NAME:
        asset_path = tool_arguments.get("assetPath", "")
        widget_name = tool_arguments.get("widgetName", "")
        texture_asset_path = tool_arguments.get("textureAssetPath", "")
        if asset_path is not None and not isinstance(asset_path, str):
            raise JsonRpcError(-32602, "ue_set_widget_image_texture.assetPath must be a string.")
        if widget_name is not None and not isinstance(widget_name, str):
            raise JsonRpcError(-32602, "ue_set_widget_image_texture.widgetName must be a string.")
        if texture_asset_path is not None and not isinstance(texture_asset_path, str):
            raise JsonRpcError(-32602, "ue_set_widget_image_texture.textureAssetPath must be a string.")
        try:
            result = build_set_widget_image_texture_tool_success(tool_arguments)
        except UeBridgeError as exc:
            result = build_set_widget_image_texture_tool_error(
                str(exc),
                exc.editor_reachable,
                str(asset_path or ""),
                str(widget_name or ""),
                str(texture_asset_path or ""),
            )
        return make_response(message_id, result)

    if tool_name == SET_GLOBAL_DEFAULT_GAME_MODE_TOOL_NAME:
        game_mode_class_path = tool_arguments.get("gameModeClassPath", "")
        if game_mode_class_path is not None and not isinstance(game_mode_class_path, str):
            raise JsonRpcError(-32602, "ue_set_global_default_game_mode.gameModeClassPath must be a string.")
        try:
            result = build_set_global_default_game_mode_tool_success(tool_arguments)
        except UeBridgeError as exc:
            result = build_set_global_default_game_mode_tool_error(
                str(exc),
                exc.editor_reachable,
                str(game_mode_class_path or ""),
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
