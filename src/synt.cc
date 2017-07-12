/*
 * FastBasic - Fast basic interpreter for the Atari 8-bit computers
 * Copyright (C) 2017 Daniel Serpell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>
 */

// Translates the syntax file to an assembly file for CA65
// -------------------------------------------------------

#include "synt-emit-asm.h"
#include "synt-parse.h"
#include "synt-wlist.h"
#include "synt-sm.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

bool p_file(parseState &p)
{
    // Output header
    std::cout << "; Syntax state machine\n\n";

    while(1)
    {
        wordlist tok(p, "TOKENS", 1);
        if( !tok.parse() )
            break;
        for(auto i: tok.map())
            std::cout << i.first << "\t= " << i.second << " * 2\n";
        std::cout << "\n";
        std::cerr << "syntax: " << tok.next() << " possible tokens.\n";
    }

    wordlist ext(p, "EXTERN", 128);
    if( ext.parse() )
    {
        int n = 128;
        for(auto i: ext.map())
            std::cout << " .global " << i.first << "\n";
        for(auto i: ext.map())
        {
            i.second = n++;
            std::cout << "SMB_" << i.first << "\t= " << i.second << "\n";
        }
        std::cout << "\nSMB_STATE_START\t= " << ext.next() << "\n\n";
    }

    std::map<std::string, std::unique_ptr<statemachine<asm_emit>>> sm_list;

    while( !p.eof() )
    {
        auto sm = std::make_unique<statemachine<asm_emit>>(p);
        if( sm->parse() )
        {
            sm_list[sm->name()] = std::move(sm);
        }
        else
        {
            sentry s(p);
            p.all();
            p.error("invalid input '" + s.str() + "'");
        }
    }
    // Emit labels table
    int ns = ext.next();
    for(auto &sm: sm_list)
        std::cout << "SMB_" << sm.second->name() << "\t= " << ns++ << "\n";
    // Emit array with addresses
    std::cout << "\nSM_TABLE_ADDR:\n";
    for(auto i: ext.map())
        std::cout << "\t.word " << i.first << " - 1\n";
    for(auto &sm: sm_list)
        std::cout << "\t.word " << sm.second->name() << " - 1\n";
    // Emit state machine tables
    std::cout << "\n";
    for(auto &sm: sm_list)
        sm.second->print();

    std::cerr << "syntax: " << (ns-128) << " tables in the parser-table.\n";
    return true;
}

static std::string readInput()
{
 std::string r;
 int c;
 while( -1 != (c = std::cin.get()) )
  r += char(c);
 return r;
}

int main()
{
 std::string inp = readInput();

 parseState ps(inp.c_str());
 p_file(ps);

 return 0;
}
