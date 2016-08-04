// Copyright (C) 2016 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#include <standardese/detail/tokenizer.hpp>

#include <cassert>
#include <string>

#include <boost/version.hpp>

#include <standardese/cpp_cursor.hpp>
#include <standardese/translation_unit.hpp>

using namespace standardese;

#if (BOOST_VERSION / 100000) != 1
#error "require Boost 1.x"
#endif

#if ((BOOST_VERSION / 100) % 1000) < 55
#warning "Boost less than 1.55 isn't tested"
#endif

void detail::skip_whitespace(token_stream& stream)
{
    while (std::isspace(stream.peek().get_value()[0]))
        stream.bump();
}

void detail::skip(token_stream& stream, const cpp_cursor& cur, const char* value)
{
    auto& val = stream.peek();
    if (val.get_value() != value)
        throw parse_error(source_location(cur), std::string("expected \'") + value + "\' got \'"
                                                    + val.get_value().c_str() + "\'");
    stream.bump();
}

void detail::skip(token_stream& stream, const cpp_cursor& cur,
                  std::initializer_list<const char*> values)
{
    for (auto val : values)
    {
        skip(stream, cur, val);
        skip_whitespace(stream);
    }
}

bool detail::skip_if_token(detail::token_stream& stream, const char* token)
{
    if (stream.peek().get_value() != token)
        return false;
    stream.bump();
    detail::skip_whitespace(stream);
    return true;
}

void detail::skip_attribute(detail::token_stream& stream, const cpp_cursor& cur)
{
    if (stream.peek().get_value() == "[" && stream.peek(1).get_value() == "[")
    {
        stream.bump(); // opening
        skip_bracket_count(stream, cur, "[", "]");
        stream.bump(); // closing
    }
    else if (skip_if_token(stream, "__attribute__"))
    {
        skip(stream, cur, "(");
        skip_bracket_count(stream, cur, "(", ")");
        skip(stream, cur, ")");
    }
}

namespace
{
    CXFile get_range(cpp_cursor cur, unsigned& begin_offset, unsigned& end_offset)
    {
        auto source = clang_getCursorExtent(cur);
        auto begin  = clang_getRangeStart(source);
        auto end    = clang_getRangeEnd(source);

        // translate location into offset and file
        CXFile file  = nullptr;
        begin_offset = 0u;
        end_offset   = 0u;
        clang_getSpellingLocation(begin, &file, nullptr, nullptr, &begin_offset);
        clang_getSpellingLocation(end, nullptr, nullptr, nullptr, &end_offset);

        return file;
    }

    std::string fixup(cpp_cursor cur, const char* ptr, std::string result, unsigned begin_offset)
    {
        auto is_templ_param = clang_getCursorKind(cur) == CXCursor_TemplateTypeParameter
                              || clang_getCursorKind(cur) == CXCursor_NonTypeTemplateParameter
                              || clang_getCursorKind(cur) == CXCursor_TemplateTemplateParameter;
        auto is_function = clang_getCursorKind(cur) == CXCursor_FunctionDecl
                           || clang_getTemplateCursorKind(cur) == CXCursor_FunctionDecl;
        auto is_class = clang_getCursorKind(cur) == CXCursor_ClassDecl
                        || clang_getCursorKind(cur) == CXCursor_StructDecl
                        || clang_getCursorKind(cur) == CXCursor_UnionDecl
                        || clang_getCursorKind(cur) == CXCursor_ClassTemplate
                        || clang_getCursorKind(cur) == CXCursor_ClassTemplatePartialSpecialization;

        // for a function, shrink to declaration only
        if (is_function && result.back() == '}')
        {
            auto body_begin = 0u;
            detail::visit_children(cur, [&](cpp_cursor child, cpp_cursor) {
                if (clang_getCursorKind(child) == CXCursor_CompoundStmt
                    || clang_getCursorKind(child) == CXCursor_CXXTryStmt)
                {
                    unsigned ignored;
                    get_range(child, body_begin, ignored);
                    return CXChildVisit_Break;
                }
                return CXChildVisit_Continue;
            });
            assert(body_begin > begin_offset);

            auto actual_size = body_begin - begin_offset;
            result.erase(result.begin() + actual_size, result.end());
            result += ';';
        }
        // for a class add semicolon
        else if (is_class && result.back() != ';')
            result += ';';

        // maximal munch fixup
        // if cur refers to a template parameter, the next character must be either '>' or comma
        // if that isn't the case, maximal munch has consumed the seperating '>' as well
        if (is_templ_param)
        {
            while (std::isspace(*ptr))
                ++ptr;

            // also need to handle another awesome libclang bug
            // everything inside the parenthesis of a decltype() isn't included
            // so need to add it
            if (*ptr == '(')
            {
                result += *ptr;
                ++ptr;

                for (auto bracket_count = 1; bracket_count != 0; ++ptr)
                {
                    if (*ptr == '(')
                        ++bracket_count;
                    else if (*ptr == ')')
                        --bracket_count;

                    result += *ptr;
                }
            }

            if (*ptr != '>' && *ptr != ',' && result.back() == '>')
                result.pop_back();
        }
        // awesome libclang bug:
        // if there is a macro expansion at the end, the closing bracket is missing
        // ie.: using foo = IMPL_DEFINED(bar
        // go backwards, if a '(' is found before a ')', append a ')'
        else if (clang_getCursorKind(cur) == CXCursor_MacroDefinition)
        {
            for (auto iter = result.rbegin(); iter != result.rend(); ++iter)
            {
                if (*iter == ')')
                    break;
                else if (*iter == '(')
                {
                    result += ')';
                    break;
                }
            }
        }
        // awesome libclang bug 2.0:
        // the extent of a function cursor doesn't cover any "= delete"
        // so append all further characters until a ';' is reached
        else if (is_function && result.back() != ';')
        {
            while (*ptr != ';')
            {
                result += *ptr;
                ++ptr;
            }
            result += ';';
        }
        // awesome libclang bug 3.0
        // the extent for a type alias is too short
        // needs to be extended until the semicolon
        else if (clang_getCursorKind(cur) == CXCursor_TypeAliasDecl && result.back() != ';')
        {
            while (*ptr != ';')
            {
                result += *ptr;
                ++ptr;
            }
            result += ';';
        }
        // prevent more awesome libclang bugs:
        // read until next line, just to be somewhat sure
        else if (clang_isDeclaration(clang_getCursorKind(cur))
                 && clang_getCursorKind(cur) != CXCursor_ParmDecl
                 && clang_getCursorKind(cur) != CXCursor_TemplateTypeParameter
                 && clang_getCursorKind(cur) != CXCursor_NonTypeTemplateParameter
                 && clang_getCursorKind(cur) != CXCursor_TemplateTemplateParameter
                 && clang_getCursorKind(cur) != CXCursor_CXXBaseSpecifier)
            while (*ptr != '\n' && *ptr)
            {
                result += *ptr;
                ++ptr;
            }

        return result;
    }
}

std::string detail::tokenizer::read_source(translation_unit& tu, cpp_cursor cur)
{
    unsigned begin_offset, end_offset;
    auto     file = get_range(cur, begin_offset, end_offset);
    if (!file)
        return "";
    assert(clang_File_isEqual(file, tu.get_cxfile()));
    assert(end_offset > begin_offset);

    auto source = tokenizer_access::get_source(tu).c_str();

    std::string result(source + begin_offset, source + end_offset);
    return fixup(cur, source + end_offset, result, begin_offset);
}

CXFile detail::tokenizer::read_range(translation_unit& tu, cpp_cursor cur, unsigned& begin_offset,
                                     unsigned& end_offset)
{
    auto file = get_range(cur, begin_offset, end_offset);

    // we need to handle the fixup, so change end_offset
    end_offset = static_cast<unsigned>(begin_offset + read_source(tu, cur).size());

    return file;
}

detail::tokenizer::tokenizer(translation_unit& tu, cpp_cursor cur)
: source_(read_source(tu, cur)), impl_(&tokenizer_access::get_context(tu))
{
    // append trailing newline
    // required for parsing code
    source_ += '\n';
}
