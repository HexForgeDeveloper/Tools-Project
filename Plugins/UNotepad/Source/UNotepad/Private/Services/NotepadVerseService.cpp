#include "Services/NotepadVerseService.h"

namespace
{
	int32 IndentWidth(const FString& Line)
	{
		int32 Width = 0;
		for (TCHAR Ch : Line)
		{
			if (Ch == TEXT(' ')) { ++Width; }
			else if (Ch == TEXT('\t')) { Width += 4 - Width % 4; }
			else { break; }
		}
		return Width;
	}
}

TArray<FNotepadVerseToken> FNotepadVerseService::TokenizeLine(const FString& Line, FNotepadVerseState& State)
{
	static const TSet<FString> Keywords = {
		TEXT("using"), TEXT("module"), TEXT("class"), TEXT("struct"), TEXT("interface"), TEXT("enum"),
		TEXT("var"), TEXT("set"), TEXT("if"), TEXT("then"), TEXT("else"), TEXT("case"), TEXT("for"),
		TEXT("loop"), TEXT("break"), TEXT("return"), TEXT("defer"), TEXT("block"), TEXT("sync"),
		TEXT("race"), TEXT("rush"), TEXT("branch"), TEXT("spawn"), TEXT("and"), TEXT("or"), TEXT("not"),
		TEXT("true"), TEXT("false"), TEXT("Self"), TEXT("super"), TEXT("where"), TEXT("external"),
		TEXT("logic"), TEXT("int"), TEXT("float"), TEXT("string"), TEXT("char"), TEXT("char32"),
		TEXT("void"), TEXT("any"), TEXT("type"), TEXT("tuple"), TEXT("array"), TEXT("map"), TEXT("option"),
		TEXT("public"), TEXT("private"), TEXT("protected"), TEXT("internal"), TEXT("override"),
		TEXT("abstract"), TEXT("concrete"), TEXT("final"), TEXT("unique"), TEXT("persistable"),
		TEXT("suspends"), TEXT("decides"), TEXT("transacts"), TEXT("computes"), TEXT("converges"),
		TEXT("reads"), TEXT("writes"), TEXT("allocates"), TEXT("no_rollback"), TEXT("native"),
		TEXT("native_callable"), TEXT("constructor"), TEXT("localizes")
	};
	TArray<FNotepadVerseToken> Tokens;
	const int32 Indent = IndentWidth(Line);
	if (State.CommentIndent != INDEX_NONE)
	{
		if (Line.TrimStartAndEnd().IsEmpty() || Indent >= State.CommentIndent)
		{
			Tokens.Add({0, Line.Len(), ENotepadVerseToken::Comment});
			return Tokens;
		}
		State.CommentIndent = INDEX_NONE;
	}
	int32 Index = 0;
	while (Index < Line.Len())
	{
		const int32 Start = Index;
		ENotepadVerseToken Kind = ENotepadVerseToken::Normal;
		if (State.BlockDepth == 0 && Line.Mid(Index, 3) == TEXT("<#>"))
		{
			State.CommentIndent = Indent + 4;
			Tokens.Add({Index, Line.Len(), ENotepadVerseToken::Comment});
			break;
		}
		if (State.BlockDepth > 0 || Line.Mid(Index, 2) == TEXT("<#"))
		{
			Kind = ENotepadVerseToken::Comment;
			do
			{
				if (Line.Mid(Index, 2) == TEXT("<#")) { ++State.BlockDepth; Index += 2; }
				else if (Line.Mid(Index, 2) == TEXT("#>")) { --State.BlockDepth; Index += 2; }
				else { ++Index; }
			} while (Index < Line.Len() && State.BlockDepth > 0);
		}
		else if (Line[Index] == TEXT('#'))
		{
			Kind = ENotepadVerseToken::Comment;
			Index = Line.Len();
		}
		else if (Line[Index] == TEXT('"') || Line[Index] == TEXT('\''))
		{
			Kind = ENotepadVerseToken::String;
			const TCHAR Quote = Line[Index++];
			while (Index < Line.Len())
			{
				if (Line[Index] == TEXT('\\')) { Index = FMath::Min(Index + 2, Line.Len()); }
				else if (Line[Index++] == Quote) { break; }
			}
		}
		else if (FChar::IsAlpha(Line[Index]) || Line[Index] == TEXT('_'))
		{
			while (Index < Line.Len() && (FChar::IsAlnum(Line[Index]) || Line[Index] == TEXT('_'))) { ++Index; }
			Kind = Keywords.Contains(Line.Mid(Start, Index - Start)) ? ENotepadVerseToken::Keyword : ENotepadVerseToken::Normal;
		}
		else if (FChar::IsDigit(Line[Index]))
		{
			Kind = ENotepadVerseToken::Number;
			while (Index < Line.Len() && FChar::IsDigit(Line[Index])) { ++Index; }
			if (Index + 1 < Line.Len() && Line[Index] == TEXT('.') && FChar::IsDigit(Line[Index + 1]))
			{
				++Index;
				while (Index < Line.Len() && FChar::IsDigit(Line[Index])) { ++Index; }
			}
			if (Index < Line.Len() && (Line[Index] == TEXT('e') || Line[Index] == TEXT('E')))
			{
				int32 Exponent = Index + 1;
				if (Exponent < Line.Len() && (Line[Exponent] == TEXT('+') || Line[Exponent] == TEXT('-'))) { ++Exponent; }
				const int32 Digits = Exponent;
				while (Exponent < Line.Len() && FChar::IsDigit(Line[Exponent])) { ++Exponent; }
				if (Exponent > Digits) { Index = Exponent; }
			}
		}
		else
		{
			Kind = FChar::IsWhitespace(Line[Index]) ? ENotepadVerseToken::Normal : ENotepadVerseToken::Punctuation;
			++Index;
		}
		Tokens.Add({Start, Index, Kind});
	}
	return Tokens;
}

FString FNotepadVerseService::NewlineIndent(const FString& TextBeforeCursor)
{
	TArray<FString> Lines;
	TextBeforeCursor.ParseIntoArray(Lines, TEXT("\n"), false);
	if (Lines.IsEmpty()) { return FString(); }
	FNotepadVerseState State;
	TArray<FNotepadVerseToken> Tokens;
	for (const FString& Line : Lines) { Tokens = TokenizeLine(Line, State); }
	const FString& LastLine = Lines.Last();
	int32 Width = IndentWidth(LastLine);
	if (State.CommentIndent != INDEX_NONE) { Width = FMath::Max(Width, State.CommentIndent); }
	else if (State.BlockDepth == 0)
	{
		for (int32 Index = Tokens.Num() - 1; Index >= 0; --Index)
		{
			const FNotepadVerseToken& Token = Tokens[Index];
			const FString Value = LastLine.Mid(Token.Begin, Token.End - Token.Begin).TrimStartAndEnd();
			if (Token.Kind == ENotepadVerseToken::Comment || Value.IsEmpty()) { continue; }
			if (Token.Kind == ENotepadVerseToken::Punctuation && (Value == TEXT(":") || Value == TEXT("=") || Value == TEXT("{"))) { Width += 4; }
			break;
		}
	}
	return FString::ChrN(Width, TEXT(' '));
}
