/*
  ==============================================================================

   This file is part of the JUCE library - "Jules' Utility Class Extensions"
   Copyright 2004-10 by Raw Material Software Ltd.

  ------------------------------------------------------------------------------

   JUCE can be redistributed and/or modified under the terms of the GNU General
   Public License (Version 2), as published by the Free Software Foundation.
   A copy of the license is included in the JUCE distribution, or can be found
   online at www.gnu.org/licenses.

   JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

  ------------------------------------------------------------------------------

   To release a closed-source product which uses JUCE, commercial licenses are
   available: visit www.rawmaterialsoftware.com/juce for more information.

  ==============================================================================
*/

#include "../../../core/juce_StandardHeader.h"

BEGIN_JUCE_NAMESPACE

#include "juce_CPlusPlusCodeTokeniser.h"
#include "../../graphics/colour/juce_Colours.h"


CPlusPlusCodeTokeniser::CPlusPlusCodeTokeniser()
{
}

CPlusPlusCodeTokeniser::~CPlusPlusCodeTokeniser()
{
}

//==============================================================================
namespace CppTokeniser
{

static bool isIdentifierStart (const juce_wchar c) throw()
{
    return CharacterFunctions::isLetter (c)
            || c == '_' || c == '@';
}

static bool isIdentifierBody (const juce_wchar c) throw()
{
    return CharacterFunctions::isLetterOrDigit (c)
            || c == '_' || c == '@';
}

static bool isReservedKeyword (const juce_wchar* const token, const int tokenLength) throw()
{
    static const juce_wchar* const keywords2Char[] =
        { T("if"), T("do"), T("or"), T("id"), 0 };

    static const juce_wchar* const keywords3Char[] =
        { T("for"), T("int"), T("new"), T("try"), T("xor"), T("and"), T("asm"), T("not"), 0 };

    static const juce_wchar* const keywords4Char[] =
        { T("bool"), T("void"), T("this"), T("true"), T("long"), T("else"), T("char"),
          T("enum"), T("case"), T("goto"), T("auto"), 0 };

    static const juce_wchar* const keywords5Char[] =
        {  T("while"), T("bitor"), T("break"), T("catch"), T("class"), T("compl"), T("const"), T("false"),
            T("float"), T("short"), T("throw"), T("union"), T("using"), T("or_eq"), 0 };

    static const juce_wchar* const keywords6Char[] =
        { T("return"), T("struct"), T("and_eq"), T("bitand"), T("delete"), T("double"), T("extern"),
          T("friend"), T("inline"), T("not_eq"), T("public"), T("sizeof"), T("static"), T("signed"),
          T("switch"), T("typeid"), T("wchar_t"), T("xor_eq"), 0};

    static const juce_wchar* const keywordsOther[] =
        { T("const_cast"), T("continue"), T("default"), T("explicit"), T("mutable"), T("namespace"),
          T("operator"), T("private"), T("protected"), T("register"), T("reinterpret_cast"), T("static_cast"),
          T("template"), T("typedef"), T("typename"), T("unsigned"), T("virtual"), T("volatile"),
          T("@implementation"), T("@interface"), T("@end"), T("@synthesize"), T("@dynamic"), T("@public"),
          T("@private"), T("@property"), T("@protected"), T("@class"), 0 };

    const juce_wchar* const* k;

    switch (tokenLength)
    {
        case 2:     k = keywords2Char; break;
        case 3:     k = keywords3Char; break;
        case 4:     k = keywords4Char; break;
        case 5:     k = keywords5Char; break;
        case 6:     k = keywords6Char; break;

        default:
            if (tokenLength < 2 || tokenLength > 16)
                return false;

            k = keywordsOther;
            break;
    }

    int i = 0;
    while (k[i] != 0)
    {
        if (k[i][0] == token[0] && CharacterFunctions::compare (k[i], token) == 0)
            return true;

        ++i;
    }

    return false;
}

static int parseIdentifier (CodeDocument::Iterator& source) throw()
{
    int tokenLength = 0;
    juce_wchar possibleIdentifier [19];

    while (isIdentifierBody (source.peekNextChar()))
    {
        const juce_wchar c = source.nextChar();

        if (tokenLength < numElementsInArray (possibleIdentifier) - 1)
            possibleIdentifier [tokenLength] = c;

        ++tokenLength;
    }

    if (tokenLength > 1 && tokenLength <= 16)
    {
        possibleIdentifier [tokenLength] = 0;

        if (isReservedKeyword (possibleIdentifier, tokenLength))
            return CPlusPlusCodeTokeniser::tokenType_builtInKeyword;
    }

    return CPlusPlusCodeTokeniser::tokenType_identifier;
}

static bool skipNumberSuffix (CodeDocument::Iterator& source)
{
    const juce_wchar c = source.peekNextChar();
    if (c == 'l' || c == 'L' || c == 'u' || c == 'U')
        source.skip();

    if (CharacterFunctions::isLetterOrDigit (source.peekNextChar()))
        return false;

    return true;
}

static bool isHexDigit (const juce_wchar c) throw()
{
    return (c >= '0' && c <= '9')
            || (c >= 'a' && c <= 'f')
            || (c >= 'A' && c <= 'F');
}

static bool parseHexLiteral (CodeDocument::Iterator& source) throw()
{
    if (source.nextChar() != '0')
        return false;

    juce_wchar c = source.nextChar();
    if (c != 'x' && c != 'X')
        return false;

    int numDigits = 0;
    while (isHexDigit (source.peekNextChar()))
    {
        ++numDigits;
        source.skip();
    }

    if (numDigits == 0)
        return false;

    return skipNumberSuffix (source);
}

static bool isOctalDigit (const juce_wchar c) throw()
{
    return c >= '0' && c <= '7';
}

static bool parseOctalLiteral (CodeDocument::Iterator& source) throw()
{
    if (source.nextChar() != '0')
        return false;

    if (! isOctalDigit (source.nextChar()))
         return false;

    while (isOctalDigit (source.peekNextChar()))
        source.skip();

    return skipNumberSuffix (source);
}

static bool isDecimalDigit (const juce_wchar c) throw()
{
    return c >= '0' && c <= '9';
}

static bool parseDecimalLiteral (CodeDocument::Iterator& source) throw()
{
    int numChars = 0;
    while (isDecimalDigit (source.peekNextChar()))
    {
        ++numChars;
        source.skip();
    }

    if (numChars == 0)
        return false;

    return skipNumberSuffix (source);
}

static bool parseFloatLiteral (CodeDocument::Iterator& source) throw()
{
    int numDigits = 0;

    while (isDecimalDigit (source.peekNextChar()))
    {
        source.skip();
        ++numDigits;
    }

    const bool hasPoint = (source.peekNextChar() == '.');

    if (hasPoint)
    {
        source.skip();

        while (isDecimalDigit (source.peekNextChar()))
        {
            source.skip();
            ++numDigits;
        }
    }

    if (numDigits == 0)
        return false;

    juce_wchar c = source.peekNextChar();
    const bool hasExponent = (c == 'e' || c == 'E');

    if (hasExponent)
    {
        source.skip();

        c = source.peekNextChar();
        if (c == '+' || c == '-')
            source.skip();

        int numExpDigits = 0;
        while (isDecimalDigit (source.peekNextChar()))
        {
            source.skip();
            ++numExpDigits;
        }

        if (numExpDigits == 0)
            return false;
    }

    c = source.peekNextChar();
    if (c == 'f' || c == 'F')
        source.skip();
    else if (! (hasExponent || hasPoint))
        return false;

    return true;
}

static int parseNumber (CodeDocument::Iterator& source)
{
    const CodeDocument::Iterator original (source);

    if (parseFloatLiteral (source))
        return CPlusPlusCodeTokeniser::tokenType_floatLiteral;

    source = original;

    if (parseHexLiteral (source))
        return CPlusPlusCodeTokeniser::tokenType_integerLiteral;

    source = original;

    if (parseOctalLiteral (source))
        return CPlusPlusCodeTokeniser::tokenType_integerLiteral;

    source = original;

    if (parseDecimalLiteral (source))
        return CPlusPlusCodeTokeniser::tokenType_integerLiteral;

    source = original;
    source.skip();

    return CPlusPlusCodeTokeniser::tokenType_error;
}

static void skipQuotedString (CodeDocument::Iterator& source) throw()
{
    const juce_wchar quote = source.nextChar();

    for (;;)
    {
        const juce_wchar c = source.nextChar();

        if (c == quote || c == 0)
            break;

        if (c == '\\')
            source.skip();
    }
}

static void skipComment (CodeDocument::Iterator& source) throw()
{
    bool lastWasStar = false;

    for (;;)
    {
        const juce_wchar c = source.nextChar();

        if (c == 0 || (c == '/' && lastWasStar))
            break;

        lastWasStar = (c == '*');
    }
}

}

//==============================================================================
int CPlusPlusCodeTokeniser::readNextToken (CodeDocument::Iterator& source)
{
    int result = tokenType_error;
    source.skipWhitespace();

    juce_wchar firstChar = source.peekNextChar();

    switch (firstChar)
    {
    case 0:
        source.skip();
        break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        result = CppTokeniser::parseNumber (source);
        break;

    case '.':
        result = CppTokeniser::parseNumber (source);

        if (result == tokenType_error)
            result = tokenType_punctuation;

        break;

    case ',':
    case ';':
    case ':':
        source.skip();
        result = tokenType_punctuation;
        break;

    case '(':
    case ')':
    case '{':
    case '}':
    case '[':
    case ']':
        source.skip();
        result = tokenType_bracket;
        break;

    case '"':
    case '\'':
        CppTokeniser::skipQuotedString (source);
        result = tokenType_stringLiteral;
        break;

    case '+':
        result = tokenType_operator;
        source.skip();

        if (source.peekNextChar() == '+')
            source.skip();
        else if (source.peekNextChar() == '=')
            source.skip();

        break;

    case '-':
        source.skip();
        result = CppTokeniser::parseNumber (source);

        if (result == tokenType_error)
        {
            result = tokenType_operator;

            if (source.peekNextChar() == '-')
                source.skip();
            else if (source.peekNextChar() == '=')
                source.skip();
        }
        break;

    case '*':
    case '%':
    case '=':
    case '!':
        result = tokenType_operator;
        source.skip();

        if (source.peekNextChar() == '=')
            source.skip();

        break;

    case '/':
        result = tokenType_operator;
        source.skip();

        if (source.peekNextChar() == '=')
        {
            source.skip();
        }
        else if (source.peekNextChar() == '/')
        {
            result = tokenType_comment;
            source.skipToEndOfLine();
        }
        else if (source.peekNextChar() == '*')
        {
            source.skip();
            result = tokenType_comment;
            CppTokeniser::skipComment (source);
        }

        break;

    case '?':
    case '~':
        source.skip();
        result = tokenType_operator;
        break;

    case '<':
        source.skip();
        result = tokenType_operator;

        if (source.peekNextChar() == '=')
        {
            source.skip();
        }
        else if (source.peekNextChar() == '<')
        {
            source.skip();

            if (source.peekNextChar() == '=')
                source.skip();
        }

        break;

    case '>':
        source.skip();
        result = tokenType_operator;

        if (source.peekNextChar() == '=')
        {
            source.skip();
        }
        else if (source.peekNextChar() == '<')
        {
            source.skip();

            if (source.peekNextChar() == '=')
                source.skip();
        }

        break;

    case '|':
        source.skip();
        result = tokenType_operator;

        if (source.peekNextChar() == '=')
        {
            source.skip();
        }
        else if (source.peekNextChar() == '|')
        {
            source.skip();

            if (source.peekNextChar() == '=')
                source.skip();
        }

        break;

    case '&':
        source.skip();
        result = tokenType_operator;

        if (source.peekNextChar() == '=')
        {
            source.skip();
        }
        else if (source.peekNextChar() == '&')
        {
            source.skip();

            if (source.peekNextChar() == '=')
                source.skip();
        }

        break;

    case '^':
        source.skip();
        result = tokenType_operator;

        if (source.peekNextChar() == '=')
        {
            source.skip();
        }
        else if (source.peekNextChar() == '^')
        {
            source.skip();

            if (source.peekNextChar() == '=')
                source.skip();
        }

        break;

    case '#':
        result = tokenType_preprocessor;
        source.skipToEndOfLine();
        break;

    default:
        if (CppTokeniser::isIdentifierStart (firstChar))
            result = CppTokeniser::parseIdentifier (source);
        else
            source.skip();

        break;
    }

    return result;
}

const StringArray CPlusPlusCodeTokeniser::getTokenTypes()
{
    const char* const types[] =
    {
        "Error",
        "Comment",
        "C++ keyword",
        "Identifier",
        "Integer literal",
        "Float literal",
        "String literal",
        "Operator",
        "Bracket",
        "Punctuation",
        "Preprocessor line",
        0
    };

    return StringArray (types);
}

const Colour CPlusPlusCodeTokeniser::getDefaultColour (const int tokenType)
{
    const uint32 colours[] =
    {
        0xffcc0000,  // error
        0xff00aa00,  // comment
        0xff0000cc,  // keyword
        0xff000000,  // identifier
        0xff880000,  // int literal
        0xff885500,  // float literal
        0xff990099,  // string literal
        0xff225500,  // operator
        0xff000055,  // bracket
        0xff004400,  // punctuation
        0xff660000   // preprocessor
    };

    if (tokenType >= 0 && tokenType < numElementsInArray (colours))
        return Colour (colours [tokenType]);

    return Colours::black;
}

bool CPlusPlusCodeTokeniser::isReservedKeyword (const String& token) throw()
{
    return CppTokeniser::isReservedKeyword (token, token.length());
}

END_JUCE_NAMESPACE
