#pragma once

#include "CoreMinimal.h"

enum class ENotepadVerseToken : uint8 { Normal, Keyword, String, Number, Comment, Punctuation };

struct FNotepadVerseToken
{
	int32 Begin;
	int32 End;
	ENotepadVerseToken Kind;
};

/** State is carried between lines, and recreated for each document pass. */
struct FNotepadVerseState
{
	int32 BlockDepth = 0;
	int32 CommentIndent = INDEX_NONE;
};

/** Lightweight lexical highlighting and editing helpers; not a Verse compiler. */
class UNOTEPAD_API FNotepadVerseService
{
public:
	static TArray<FNotepadVerseToken> TokenizeLine(const FString& Line, FNotepadVerseState& State);
	static FString NewlineIndent(const FString& TextBeforeCursor);
};
