#include "MCPSidebarGameMode.h"

#include "MCPSidebarPlayerController.h"

AMCPSidebarGameMode::AMCPSidebarGameMode()
{
	PlayerControllerClass = AMCPSidebarPlayerController::StaticClass();
	UE_LOG(LogTemp, Log, TEXT("[MCPSidebar] GameMode constructed. PlayerControllerClass=%s"), *GetNameSafe(PlayerControllerClass));
}
