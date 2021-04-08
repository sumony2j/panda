// SPDX-License-Identifier: BSD-2-Clause-FreeBSD
/*
 * Copyright (c) 2021 SiPanda Inc.
 *
 * Authors: Felipe Magno de Almeida <felipe@expertise.dev>
 *          João Paulo Taylor Ienczak Zanette <joao.tiz@expertise.dev>
 *          Lucas Cavalcante de Sousa <lucas@expertise.dev>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <iostream>
#include <numeric>
#include <string>

#include <boost/wave.hpp>
#include <boost/wave/cpplexer/cpp_lex_iterator.hpp>

#include "pandagen/code.h"
#include "pandagen/graph.h"
#include "pandagen/macro_defs.h"

namespace pandagen
{

template <typename G> struct MacroOnly :
		boost::wave::context_policies::default_preprocessing_hooks {
	typedef typename boost::graph_traits<G>::vertex_descriptor
							vertex_descriptor;
	typedef typename boost::graph_traits<G>::edge_descriptor
							edge_descriptor;

	std::vector<std::tuple<std::string, vertex_descriptor, bool>> *roots;
	std::vector<table> *parser_tables;
	std::vector<tlv_node> *tlv_nodes;
	std::vector<table> *tlv_tables;
	G *graph;

	MacroOnly(G &g, std::vector<table> &parser_tables,
		  std::vector<table> &tlv_tables,
                  std::vector<tlv_node> &tlv_nodes,
		  std::vector<std::tuple<std::string, vertex_descriptor, bool>> &roots)
		: boost::wave::context_policies::default_preprocessing_hooks{},
		  graph{ &g }, parser_tables (&parser_tables),
		  tlv_tables (&tlv_tables), tlv_nodes{ &tlv_nodes },
		  roots (&roots)
	{
	}

	// Ignores #include directives
	template <typename ContextType> bool
	found_include_directive(const ContextType &context,
				const std::string &filename, bool include_next)
	{
		return true;
	}

	// Output function like macro info
	template <typename ContextT, typename TokenT, typename ContainerT,
		  typename IteratorT> bool
	expanding_function_like_macro(ContextT const &ctx,
				      TokenT const &macro_name,
				      std::vector<TokenT> const &parameters,
				      ContainerT const &definition,
                                      TokenT const &macrocall,
				      std::vector<ContainerT> const &arguments,
				      IteratorT const &seqstart,
				      IteratorT const &seqend)
	{
		auto macro = macro_name.get_value();

		if (macro == "PANDA_DECL_PARSE_NODE" ||
		    macro == "PANDA_DECL_TLVS_PARSE_NODE") {
			pandagen::handle_decl_node(*graph, arguments);
		} else if (macro == "PANDA_MAKE_PROTO_TABLE") {
			pandagen::handle_make_table(*graph, *parser_tables,
						   arguments);
		} else if (macro == "PANDA_MAKE_TLV_TABLE") {
			pandagen::handle_make_table(*graph, *tlv_tables,
						    arguments);
		} else if (macro == "PANDA_MAKE_TLV_PARSE_NODE") {
			pandagen::handle_make_tlv_node(*tlv_nodes, arguments);
		} else if (macro == "PANDA_MAKE_LEAF_PARSE_NODE") {
			pandagen::handle_make_leaf_node(*graph, arguments);
		} else if (macro == "PANDA_MAKE_LEAF_TLVS_PARSE_NODE") {
			pandagen::handle_make_leaf_tlv_node(*graph, arguments);
		} else if (macro_name.get_value() == "PANDA_MAKE_PARSE_NODE") {
			pandagen::handle_make_node(*graph, arguments);
		} else if (macro_name.get_value() == "PANDA_PARSER_ADD") {
			pandagen::handle_parser_add(*graph, *roots, arguments);
		} else if (macro_name.get_value() == "PANDA_PARSER") {
			pandagen::handle_parser(*graph, *roots, arguments);
                }
		return true;
	}

	template <typename TokenContainer> static std::vector<std::string>
	string_tokens (const TokenContainer &tokens)
	{
		auto strings = std::vector<std::string>{};

		for (const auto &token : tokens) {
			auto str = to_std_string(token.get_value());

			if (is_whitespace(str))
				continue;

			strings.push_back(str);
		}

		return strings;
	}

	template <typename ContainerT> void
	table_from_macro_args(vertex_descriptor source,
			      std::vector<ContainerT> const &arguments)
	{
	}
};

template <typename Context> auto
add_panda_macros (Context &context)
{
	auto macros = std::vector<std::string>{
		"PANDA_DECL_PARSE_NODE(node)",
		"PANDA_DECL_TLVS_PARSE_NODE(node)",
		"PANDA_MAKE_PROTO_TABLE(table_name, ...)",
		"PANDA_MAKE_TLV_TABLE(table_name, ...)",
		"PANDA_MAKE_TLV_PARSE_NODE(node, name, metadata, pointer)",
		"PANDA_MAKE_PARSE_NODE(node, name, metadata, pointer, table)",
		"PANDA_MAKE_TLVS_PARSE_NODE(node, name, metadata, pointer, "
		"table)",
		"PANDA_MAKE_LEAF_PARSE_NODE(node, name, metadata, pointer)",
		"PANDA_MAKE_LEAF_TLVS_PARSE_NODE(node, name, metadata, "
		"pointer, another_pointer, table)",
		"PANDA_PARSER_ADD(name, description, node_addr)",
		"PANDA_PARSER(parser, description, node_addr)",
	};

	for (const auto &macro : macros)
		context.add_macro_definition (macro, true);
}

template <typename G> void
parse_file(G &g, std::vector<std::tuple<std::string,
	   typename boost::graph_traits<G>::vertex_descriptor, bool>> &roots,
	   std::string filename)
{
	// save current file position for exception handling
	using position_type = boost::wave::util::file_position_type;
	position_type current_position;

	try {
		using lex_iterator_type = boost::wave::cpplexer::
				lex_iterator<boost::wave::cpplexer::
				lex_token<> >;
		using input_policy = boost::wave::iteration_context_policies::
				load_file_to_string;
		using context_policy = MacroOnly<G>;

                using context_type = boost::wave::context<std::string::iterator,
			lex_iterator_type, input_policy, context_policy>;

		auto file = std::ifstream(filename);

                auto input = std::string(std::istreambuf_iterator<char>
					(file.rdbuf()),
					std::istreambuf_iterator<char>());

		std::vector<table> parser_tables;
		std::vector<tlv_node> tlv_nodes;
		std::vector<table> tlv_tables;
		context_type context(input.begin(), input.end(),
				     filename.c_str (),
				     MacroOnly<G>{g, parser_tables, tlv_tables,
						  tlv_nodes, roots});

		add_panda_macros(context);
		for (const auto &it : context)
			current_position = it.get_position();

		std::cout << "proto tables size: " << parser_tables.size () <<
			" tlv tables size " << tlv_tables.size () <<
			" tlv nodes " << tlv_nodes.size () << std::endl;

		pandagen::connect_vertices (g, parser_tables);
		pandagen::fill_tlv_node_to_vertices (g, tlv_nodes, tlv_tables);
	}

	// preprocessing error
	catch (boost::wave::cpp_exception const &e) {
                std::cerr << e.file_name () << "(" << e.line_no () <<
			") preprocessing error: " << e.description () <<
			std::endl;
	}
	// use last recognized token to retrieve the error position
	catch (std::exception const &e) {
		std::cerr << current_position.get_file () << "(" <<
			current_position.get_line () << "): " <<
			"exception caught: " << e.what () << std::endl;
	}
	// use last recognized token to retrieve the error position
	catch (...) {
		std::cerr << current_position.get_file () << "(" <<
			current_position.get_line () << "): " <<
			"unexpected exception caught." << std::endl;
	}
}

} // namespace pandagen

int main (int argc, char *argv[])
{
	if (argc != 2 && argc != 3) {
		std::cout << "Usage: " << argv[0] << " <source> [OUTPUT]\n" <<
			"\n"
			"Where if OUTPUT is provided:\n"
			"  - If OUTPUT extension is .c, "
			"generates C code\n"
			"  - If OUTPUT extension is .dot, "
			"generates graphviz dot file\n";
		return 1;
	}

	using graph_t = boost::adjacency_list<boost::vecS, boost::vecS,
		boost::directedS, pandagen::vertex_property,
		pandagen::edge_property, boost::no_property, boost::vecS>;
	graph_t graph;

	std::vector<std::tuple<std::string,
		    boost::graph_traits<graph_t>::vertex_descriptor, bool>> roots;
	pandagen::parse_file(graph, roots, argv[1]);

	{
		auto vs = vertices (graph);
		std::cout << "Finished parsing file. " <<
			std::distance (vs.first, vs.second) << " vertices\n";
	}

	if (!roots.empty()) {
		auto back_edges = pandagen::back_edges(graph, get<1>(roots[0]));

		for (auto &&edge : back_edges) {
			auto u = source(edge, graph);
			auto v = target(edge, graph);

			std::cout << "  [" << graph[u].name << ", " <<
				graph[v].name << "]\n";
		}

		std::cout << "Has cycle? -> " <<
				(back_edges.empty () ? "No" : "Yes") << "\n";

		if (argc == 3) {
			auto output = std::string{ argv[2] };

			if (output.substr(std::max(output.size() - 4,
					  0ul)) == ".dot") {
				std::cout << "Generating dot file...\n";
				pandagen::dotify(graph, output,
						 get<1>(roots[0]), back_edges);
			} else if (output.substr(std::max(output.size() - 2,
						 0ul)) == ".c") {
				auto file = std::ofstream { output };
				auto header_name = output.substr(0,
						output.size() - 2) + ".h";
				auto header = std::ofstream { header_name };
				auto out = std::ostream_iterator<char>(file);
				auto hout = std::ostream_iterator<char>(header);
				generate_parsers(out, graph, argv[1],
						 header_name);
				std::cout << "header name " << header_name <<
								std::endl;
				for (auto&& root : roots) {
					pandagen::generate_root_parser(
					     std::ostream_iterator<char>(file),
					     graph, get<1>(root), get<0>(root),
					     argv[1], hout, get<2>(root));
				}
			} else {
				std::cout << "Unknown file extension in "
					     "filename " << output << ".\n";
				return 1;
			}
			std::cout << "Done\n";
		} else {
			std::cout << "Nothing to generate\n";
		}
	} else {
		std::cout << "No roots in this parser, use PANDA_PARSER_ADD" <<
			     "or PANDA_PARSER" << std::endl;
	}
}