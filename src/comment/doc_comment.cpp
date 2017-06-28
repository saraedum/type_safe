// Copyright (C) 2016-2017 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#include <standardese/comment/doc_comment.hpp>

#include <cassert>

#include <standardese/markup/entity_kind.hpp>

using namespace standardese;
using namespace standardese::comment;

namespace
{
    template <class Builder>
    void set_sections_impl(Builder& builder, const doc_comment& comment)
    {
        if (comment.brief_section())
            builder.add_brief(markup::clone(comment.brief_section().value()));

        for (auto& sec : comment.sections())
        {
            auto ptr = sec.clone().release();
            if (ptr->kind() == markup::entity_kind::details_section)
                builder.add_details(std::unique_ptr<markup::details_section>(
                    static_cast<markup::details_section*>(ptr)));
            else if (ptr->kind() == markup::entity_kind::inline_section)
                builder.add_section(std::unique_ptr<markup::inline_section>(
                    static_cast<markup::inline_section*>(ptr)));
            else if (ptr->kind() == markup::entity_kind::list_section)
                builder.add_section(
                    std::unique_ptr<markup::list_section>(static_cast<markup::list_section*>(ptr)));
            else
                assert(false);
        }
    }
}

void ::standardese::comment::set_sections(
    standardese::markup::entity_documentation::builder& builder, const doc_comment& comment)
{
    set_sections_impl(builder, comment);
}

void ::standardese::comment::set_sections(standardese::markup::file_documentation::builder& builder,
                                          const doc_comment&                                comment)
{
    set_sections_impl(builder, comment);
}
