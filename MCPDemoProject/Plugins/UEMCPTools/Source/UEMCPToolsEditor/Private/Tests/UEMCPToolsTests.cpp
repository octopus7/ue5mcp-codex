#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UEMCPImportTools.h"
#include "UEMCPToolRegistry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEMCPToolRegistryExecuteTest,
	"UEMCPTools.Registry.ExecuteKnownTool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEMCPToolRegistryExecuteTest::RunTest(const FString& Parameters)
{
	FMCPToolRegistry Registry;
	Registry.RegisterTool(
		TEXT("test.tool"),
		[](const FMCPInvokeRequest& Request, FMCPInvokeResponse& Response)
		{
			Response.RequestId = Request.RequestId;
			Response.AppliedCount += 1;
			return true;
		});

	FMCPInvokeRequest Request;
	Request.RequestId = TEXT("req-001");
	Request.ToolId = TEXT("test.tool");

	FMCPInvokeResponse Response;
	const bool bSuccess = Registry.Execute(Request, Response);
	TestTrue(TEXT("Execute should succeed for registered tool"), bSuccess);
	TestEqual(TEXT("AppliedCount should increment"), Response.AppliedCount, 1);
	TestEqual(TEXT("RequestId should flow through"), Response.RequestId, FString(TEXT("req-001")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEMCPImportRuleOrderTest,
	"UEMCPTools.Import.RuleOrderFirstMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEMCPImportRuleOrderTest::RunTest(const FString& Parameters)
{
	TArray<FMCPImportRule> Rules;

	FMCPImportRule FirstRule;
	FirstRule.FolderPath = TEXT("/Game/UI");
	FirstRule.bRecursive = true;
	FirstRule.bEnabled = true;
	Rules.Add(FirstRule);

	FMCPImportRule SecondRule;
	SecondRule.FolderPath = TEXT("/Game/UI/Textures");
	SecondRule.bRecursive = true;
	SecondRule.bEnabled = true;
	Rules.Add(SecondRule);

	const int32 MatchIndex = FUEMCPImportTools::FindFirstMatchingRuleIndex(TEXT("/Game/UI/Textures"), Rules);
	TestEqual(TEXT("First matching rule should win by registration order"), MatchIndex, 0);

	return true;
}

#endif
