/*
 * FastBasic - Fast basic interpreter for the Atari 8-bit computers
 * Copyright (C) 2017-2019 Daniel Serpell
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

// parser.h: C++ parser

#pragma once

#include "codew.h"
#include "looptype.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <iostream>

extern bool do_debug;

class parse {
    public:
        class saved_pos {
            public:
                size_t pos;
                size_t opos;
        };
        class jump {
            public:
                LoopType type;
                std::string label;
                int linenum;
        };
        int sto_var;
        int lvl, maxlvl;
        std::string str;
        size_t pos;
        size_t max_pos;
        std::vector<std::string> saved_errors;
        int linenum;
        std::map<std::string, std::vector<codew>> procs;
        std::vector<std::string> proc_stack;
        std::map<std::string, int> vars;
        std::map<std::string, int> labels;
        std::vector<jump> jumps;
        int label_num;
        bool finalized;
        std::vector<codew> *code;

        parse():
            sto_var(-1),
            lvl(0), maxlvl(0), pos(0),
            max_pos(0), linenum(0), label_num(0),
            finalized(false),
            code(&procs[std::string()]) { }

        std::string new_label()
        {
            label_num++;
            return "jump_lbl_" + std::to_string(label_num);
        }
        std::string push_loop(LoopType type)
        {
            auto lbl = new_label();
            jumps.push_back({type, lbl, linenum});
            return lbl;
        }
        bool peek_loop(LoopType type)
        {
            if( !jumps.size() )
                return false;
            auto last = jumps.back();
            return !( last.type != type && ( type != LT_ELSE || last.type != LT_IF ) );
        }
        std::string pop_loop(LoopType type)
        {
            if( !jumps.size() )
            {
                if( type == LT_ELSE || type == LT_ELIF )
                    type = LT_IF;
                loop_error("missing " + get_loop_name(type));
                return std::string();
            }
            auto last = jumps.back();
            if( last.type != type )
            {
                if( type !=  LT_ELSE || last.type != LT_IF )
                {
                    if( type == LT_ELSE || type == LT_ELIF )
                        type = LT_IF;
                    loop_error("missing " + get_loop_name(type));
                    return std::string();
                }
            }
            auto lbl = last.label;
            jumps.pop_back();
            return lbl;
        }
        std::string check_loops()
        {
            // Checks that there are no unclosed loops at the end
            // of compilation.
            if( !jumps.size() )
                return std::string();
            for( ; jumps.size(); jumps.pop_back() )
            {
                auto j = jumps.back();
                auto type = j.type;
                if( type != LT_EXIT )
                    return "unclosed " + get_loop_name(type) + " at line " + std::to_string(j.linenum);
            }
            return "EXIT without loop";
        }

        void new_line(std::string l, int ln)
        {
            pos = max_pos = 0;
            str = l;
            saved_errors.clear();
            linenum = ln;
        }

        saved_pos save()
        {
            return saved_pos{pos, code->size()};
        }

        void error(std::string str)
        {
            if( !str.empty() )
            {
                if( pos >= max_pos )
                {
                    debug( "Set error='" + str + "' "
                           "at pos='" + std::to_string(pos) + "' "
                           "mp='" + std::to_string(max_pos) + "'"  );
                    if( pos > max_pos )
                    {
                        debug( "ERROR SAVED" );
                        saved_errors.clear();
                    }
                    else
                        debug( "ERROR ADDED" );
                    if( saved_errors.end() == std::find(saved_errors.begin(), saved_errors.end(), str) )
                        saved_errors.push_back(str);
                    max_pos = pos;
                }
            }
        }

        bool loop_error(std::string str)
        {
            // Loop error takes precedence over all other errors
            saved_errors.clear();
            saved_errors.push_back(str);
            debug( "Set loop error='" + str + "'" );
            return false;
        }

        void restore(saved_pos s)
        {
            if( pos > max_pos )
                debug("error, pos > max_pos ?!?!?");
            if( pos != s.pos )
                debug("restore pos=" + std::to_string(pos) + " <= " + std::to_string(s.pos));
            pos = s.pos;
            code->resize(s.opos, codew::ctok(TOK_END, 0));
        }

        codew remove_last()
        {
            codew ret = code->back();
            code->pop_back();
            return ret;
        }
        void debug(const std::string &c)
        {
            if(do_debug)
            {
                if( lvl > maxlvl ) maxlvl = lvl;
                for(int i=0; i<lvl; i++)
                    std::cout << " ";
                std::cout << c << "\n";
                std::cout.flush();
            }
        }

        bool eos()
        {
            return pos >= str.length();
        }

        bool range(char c1, char c2)
        {
            if( pos < str.length() )
            {
                if( str[pos] >= c1 && str[pos] <= c2 )
                {
                    pos ++;
                    return true;
                }
            }
            return false;
        }
        bool ident_start()
        {
            return pos < str.length() &&
                ( (str[pos] >= 'a' && str[pos] <= 'z') ||
                  (str[pos] >= 'A' && str[pos] <= 'Z') || str[pos] == '_' );
        }
        bool get_ident(std::string &ret)
        {
            skipws();
            if( ident_start() )
            {
                while( pos < str.length() )
                {
                    char c = str[pos];
                    if( c >= 'a' && c <= 'z' )
                        c = c - ('a' - 'A');
                    if( (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' )
                    {
                        ret += c;
                        pos ++;
                    }
                    else
                        break;
                }
                skipws();
                return true;
            }
            else
                return false;
        }
        bool peek(char c)
        {
            if( pos < str.length() )
            {
                char p = str[pos];
                if( p >= 'a' && p >= 'z' )
                    p = p - ('a' - 'A');
                if( c >= 'a' && c <= 'z' )
                    c = c - ('a' - 'A');
                if( p == c )
                {
                    return true;
                }
            }
            return false;
        }
        bool skipws()
        {
            while( pos < str.length() && (str[pos] == ' ' || str[pos] == '\t') )
            {
                pos++;
                if( pos  > max_pos )
                    max_pos = pos;
            }
            return true;
        }
        bool expect(char c)
        {
            if( pos < str.length() )
            {
                char p = str[pos];
                if( p >= 'a' && p <= 'z' )
                    p = p - ('a' - 'A');
                if( c >= 'a' && c <= 'z' )
                    c = c - ('a' - 'A');
                if( p == c )
                {
                    pos ++;
                    return true;
                }
            }
            // Add left parenthesis as possible error, provides better messages
            if (c == ')')
                error("right parenthesis");
            else if (c == ']')
                error("right bracket");
            return false;
        }
        bool eol()
        {
            if( pos < str.length() )
            {
                // Three types of EOL:
                if( (str[pos] == '\x9B') || // AT-ASCII EOL
                    (str[pos] == '\n') )  // Unix EOL
                {
                    pos ++;
                    return true;
                }
                // Windows EOL, two bytes (last is :
                if( (str[pos] == '\r')  && (pos < str.length()-1) && (str[pos+1] == '\n') )
                {
                    pos += 2;
                    return true;
                }
            }
            return false;
        }
        bool emit_word(std::string s)
        {
            code->push_back(codew::cword(s, linenum));
            return true;
        }
        bool emit_word(int x)
        {
            code->push_back(codew::cword(x, linenum));
            return true;
        }
        bool emit_fp(atari_fp x)
        {
            code->push_back(codew::cfp(x, linenum));
            return true;
        }
        bool emit_label(std::string s)
        {
            code->push_back(codew::clabel(s, linenum));
            return true;
        }
        bool emit_tok(enum tokens tk)
        {
            code->push_back(codew::ctok(tk, linenum));
            return true;
        }
        bool emit_str(std::string s)
        {
            code->push_back(codew::cstring(s, linenum));
            return true;
        }
        bool emit_byte(std::string s)
        {
            code->push_back(codew::cbyte(s, linenum));
            return true;
        }
        bool emit_byte(int x)
        {
            code->push_back(codew::cbyte(x, linenum));
            return true;
        }
        void push_proc(std::string l)
        {
            proc_stack.push_back(l);
            code = &procs[l];
        }
        void pop_proc(std::string l)
        {
            if( !proc_stack.size() )
                throw std::runtime_error("empty proc stack");
            if( proc_stack.back() != l )
                throw std::runtime_error("invalid proc stack");
            proc_stack.pop_back();
            if( !proc_stack.size() )
                code = &procs[std::string()];
            else
                code = &procs[proc_stack.back()];
        }
        std::vector<codew> &full_code()
        {
            std::vector<codew> &p = procs[std::string()];
            if( !finalized )
            {
                finalized = true;
                // Correctly terminate main code
                if( !p.size() || !p.back().is_tok(TOK_END) )
                    p.push_back(codew::ctok(TOK_END,0));
                // To emit procs sorted by line number, copy to a vector
                std::vector< std::vector<codew>* > sprocs;
                for(auto &c: procs)
                    if( !c.first.empty() && c.second.size() )
                        sprocs.push_back( &c.second );
                // Sort by line number
                std::sort(std::begin(sprocs), std::end(sprocs),
                        [](const std::vector<codew>* a,
                           const std::vector<codew>* b)
                        {
                           return (*a)[0].linenum() < (*b)[0].linenum();
                        });
                // Emit into code
                for(auto &c: sprocs)
                    p.insert(std::end(p), std::begin(*c), std::end(*c));
            }
            return p;
        }
        std::vector<enum tokens> used_tokens()
        {
            auto code = full_code();
            std::set<enum tokens> set;
            for(auto &c: code)
            {
                if( c.is_tok() )
                    set.insert(c.get_tok());
            }
            std::vector<enum tokens> ret;
            for(auto &t: set)
                ret.push_back(t);
            return ret;
        }
};


