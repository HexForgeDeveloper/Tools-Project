#include "Services/NotepadVerseService.h"
#include "Services/NotepadDocumentUtils.h"
#include "Services/NotepadTextActionService.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNotepadVerseCommentsTest, "UNotepad.Verse.Comments", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNotepadVerseCommentsTest::RunTest(const FString& Parameters)
{
	FNotepadVerseState State;
	FNotepadVerseService::TokenizeLine(TEXT("<# outer <# nested"), State);
	TestEqual(TEXT("Nested comment depth"), State.BlockDepth, 2);
	auto Tokens = FNotepadVerseService::TokenizeLine(TEXT("#> still comment #> var X : int = 1"), State);
	TestEqual(TEXT("Comment closes across lines"), State.BlockDepth, 0);
	TestEqual(TEXT("Comment ends at outer delimiter"), Tokens[0].End, 19);
	TestTrue(TEXT("Code resumes after comment"), Tokens.ContainsByPredicate([](const FNotepadVerseToken& T) { return T.Kind == ENotepadVerseToken::Keyword; }));
	FNotepadVerseService::TokenizeLine(TEXT("    <#>"), State);
	Tokens = FNotepadVerseService::TokenizeLine(TEXT("        var ignored = 1"), State);
	TestEqual(TEXT("Indented comment covers whole line"), Tokens.Num(), 1);
	TestEqual(TEXT("Indented content is a comment"), Tokens[0].Kind, ENotepadVerseToken::Comment);
	FNotepadVerseService::TokenizeLine(TEXT(""), State);
	Tokens = FNotepadVerseService::TokenizeLine(TEXT("    var Visible = 1"), State);
	TestEqual(TEXT("Dedent ends comment"), State.CommentIndent, INDEX_NONE);
	Tokens = FNotepadVerseService::TokenizeLine(TEXT("\"# <# not a comment\" # actual comment"), State);
	TestEqual(TEXT("Markers inside string stay string"), Tokens[0].Kind, ENotepadVerseToken::String);
	TestEqual(TEXT("Trailing hash starts comment"), Tokens.Last().Kind, ENotepadVerseToken::Comment);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNotepadVerseEditingTest, "UNotepad.Verse.Editing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNotepadVerseEditingTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Verse detection is case insensitive"), FNotepadDocumentUtils::DetectModeFromPath(TEXT("Device.VERSE")), EUNotepadDocumentMode::Verse);
	TestEqual(TEXT("New document extension"), FNotepadDocumentUtils::GetUntitledName(EUNotepadDocumentMode::Verse, 1), FString(TEXT("Untitled-1.verse")));
	TestEqual(TEXT("Block indentation with trailing comment"), FNotepadVerseService::NewlineIndent(TEXT("    if (Ready?): # test")), FString(TEXT("        ")));
	TestEqual(TEXT("Function body indentation"), FNotepadVerseService::NewlineIndent(TEXT("Run():void =")), FString(TEXT("    ")));
	TestEqual(TEXT("Ignore colon in string"), FNotepadVerseService::NewlineIndent(TEXT("    \"label:\"")), FString(TEXT("    ")));
	TestEqual(TEXT("Ignore colon in multiline comment"), FNotepadVerseService::NewlineIndent(TEXT("<# comment\n    if:")), FString(TEXT("    ")));
	TestEqual(TEXT("Indented comment body"), FNotepadVerseService::NewlineIndent(TEXT("    <#>")), FString(TEXT("        ")));
	FString Text = TEXT("    var X : int = 1\n    set X = 2");
	const FString Original = Text;
	FNotepadTextActionService::ToggleLineComment(Text, 0, 1, TEXT("#"));
	TestTrue(TEXT("Uses Verse comment marker"), Text.StartsWith(TEXT("    #")));
	FNotepadTextActionService::ToggleLineComment(Text, 0, 1, TEXT("#"));
	TestEqual(TEXT("Comment toggle preserves indentation"), Text, Original);
	FNotepadVerseState State;
	const auto Tokens = FNotepadVerseService::TokenizeLine(TEXT("1..5+2.5e-3"), State);
	TestEqual(TEXT("Range not swallowed into number"), Tokens[0].End, 1);
	TestEqual(TEXT("Exponent belongs to final number"), Tokens.Last().End, 11);
	return true;
}
#endif
