#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>
#include <cctype>
#include <string>
#include <ostream>
#include <sstream>
#include <cassert>
#include <algorithm>

using namespace std;

class BacktrackGuard
{
public:
	explicit BacktrackGuard(const char* &pos):
		pos(pos), start(pos){}
	~BacktrackGuard(){
		if(start)
			pos = start;
	}

	void no_backtrack() {
		start = 0;
	}

private:
	const char* &pos;
	const char* start;

	BacktrackGuard(const BacktrackGuard&);
	BacktrackGuard &operator=(const BacktrackGuard&);
};

class VarManager
{
public:
	bool create_var(string name, ostream &out);
	bool is_var_defined(string name) const;
	string get_var_address(string name) const;
	void push_scope();
	void pop_scope(ostream &out);

private:
	vector<string> var_name;
	vector<unsigned>scope_start;

	bool VarManager::create_var(string name, ostream &out)
	{
		assert(!scope_start.empty());
		if(std::find(var_name.begin() + scope_start.back(), var_name.end(), name) == var_name.end())
		{
			var_name.push_back(name);
			out << "subl $4, %esp" << endl;
			return true;
		}
		else
		{
			return false;
		}
	}

	bool VarManager::is_var_defined(string name)const
	{
		assert(!scope_start.empty());
		return std::find(var_name.begin(), var_name.end(), name) != var_name.end();
	}

	string VarManager::get_var_address(string name)const
	{
		assert(!scope_start.empty());
		for(unsigned i_plus_one = var_name.size(); i_plus_one > 0; --i_plus_one)
		{
			if(var_name[i_plus_one-1] == name)
			{
				ostringstream out;
				out << "-" << 4 * (i_plus_one) << "%(ebp)";
				return out.str();
			}
		}
		assert(false);
	}

	void VarManager::push_scope()
	{
		scope_start.push_back(var_name.size());
	}

	void VarManager::pop_scope(ostream &out)
	{
		assert(!scope_start.empty());
		unsigned to_pop = 4 * (var_name.size() - scope_start.back());
		if(to_pop != 0)
		{
			out << "addl $" << to_pop << ", %esp" << endl;
			var_name.erase(var_name.begin() + scope_start.back(), var_name.end());
		}
		scope_start.pop_back();
	}
};

class LabelManager
{
public:
	LabelManager();
	string make_unique();
private:
	unsigned i;

	LabelManager::LabelManager():i(0)
	{
	}
	string LabelManager::make_unique()
	{
		ostringstream out;
		out << "L" << i++;
		return out.str();
	}
};

class DataSection
{
public:
	void add_string_data(string label, string data);
	void write_data_sections(ostream&);
};

class Env
{
public:
	explicit Env(VarManager &var, ostream &out, DataSection &data, LabelManager &label):
			var(var), out(out), data(data), label(label){}

	VarManager &var;
	ostream &out;
	DataSection &data;
	LabelManager &label;
};

void skip_spaces(const char* &pos, const char* end)
{
	while(pos != end && isspace(*pos))
		++pos;
}

bool ignore(const char* pos, const char* end, const char *what)
{
	BacktrackGuard guard(pos);
	skip_spaces(pos, end);
	while(pos != end && *pos == *what)
	{
		++what;
		++pos;
		if(*what == '\0')
		{
			--what;
			if(isalpha(*what))
			{
				if(isalnum(*pos))
				{
					return false;
				}
			}
			guard.no_backtrack();
			return true;
		}
	}
	return false;
}

void write_print_one_code(std::ostream &out)
{
	out << "pushl $1" << endl << "call _print_int" << endl << "addl $4, %esp" << endl;
}

bool read_number(const char* &pos, const char* end, int &n)
{
	skip_spaces(pos, end);
	if(pos != end && isdigit(*pos))
	{
		n = 0;
		do
		{
			n *= 10;
			n += *pos - '0';
			++pos;
		}while(pos != end && isdigit(*pos));
		return true;
	}
	else
	{
		return false;
	}
}

char unescape_character(char c)
{
	switch(c)
	{
	case 'n': return '\n';
	case 't': return '\t';
	case '\\': return '\\';
	case '\"': return '\"';
	}
	cerr << "Unknown escape Sequence \\" << c << endl;
	return c;
}

bool read_string(const char* &pos, const char* end, string &str)
{
	BacktrackGuard guard(pos);
	skip_spaces(pos, end);
	if(pos != end && *pos == '"')
	{
		++pos;
		str = "";
		do
			{
				if(*pos == '\\')
				{
					++pos;
					if(pos == end)
					{
						cerr << "EOF in string literal" << endl;
						return false;
					}
					str += unescape_character(*pos);
				}
				else
				{
					str += *pos;
					++pos;
				}
			}while(pos != end && *pos != '\"');
			++pos;
			guard.no_backtrack();
			return true;
	}
	else
	{
		return false;
	}

}

bool parse_literal(Env &env, const char* &pos, const char* end)
{
	int n;
	if(read_number(pos, end, n))
	{
		env.out << "movl $" << n << ", %eax" << endl;
		return true;
	}
	string identifier;
	if(read_identifier(pos, end, identifier))
	{
		if(env.var.is_var_defined(identifier))
		{
			env.out << "movl" << env.var.get_var_address(identifier) << ", %eax" << endl;
			return true;
		}
		else
		{
			cerr << "Undefiend variable " << identifier << endl;
			return false;
		}
	}
	return false;
}

bool parse_factor(const char* &pos, const char* end, Env &env)
{
	BacktrackGuard guard(pos);
	int n;
	bool negate = false;

	if(ignore(pos, end, "-"))
	{
		negate = true;
	}

	if(parse_literal(env, pos, end))
	{

	}
	else if(ignore(pos, end, "("))
	{
		parse_expression(pos, end, env);
		if(!ignore(pos, end, ")"))
		{
			cerr << "Missing )" << endl;
		}
	}
	else
	{
		return false;
	}

	if(negate)
	{
		env.out << "negl %eax" << endl;
	}
	guard.no_backtrack();
	return true;
}

bool parse_term(const char* &pos, const char* end, Env &env)
{
	BacktrackGuard guard(pos);
	if(parse_factor(pos, end, env))
	{
		for(;;)
		{
			if(ignore(pos, end, "*"))
			{
				env.out << "push %eax" << endl;
				if(!parse_factor(pos, end, env))
				{
					cerr << "Expected factor after operator" << endl;
					return false;
				}
				env.out << "imull (%esp), %eax" << endl << "addl $4, %esp" << endl;
			}
			else if(ignore(pos, end, "/"))
			{
				env.out << "push %eax" << endl;
				if(!parse_factor(pos, end, env))
				{
					cerr << "Expected factor after operator" << endl;
					return false;
				}
				env.out << "movl %eax, %ebx" << endl << "pop %eax" << endl << "movl $0, %edx" << endl << "idiv %ebx" << endl;
			}
			else if(ignore(pos, end, "%"))
			{
				env.out << "push %eax" << endl;
				if(!parse_factor(pos, end, env))
				{
					cerr << "Expected factor after operator" << endl;
					return false;
				}
				env.out << "movl %eax, %ebx" << endl << "pop %eax" << endl << "movl $0, %edx" << endl << "idiv %ebx" << endl << "movl %edx, %eax" << endl;
 			}
			else
			{
				break;
			}
		}
		guard.no_backtrack();
		return true;
	}
	else
	{
		return false;
	}
}

bool parse_expression(const char* &pos, const char* end, Env &env)
{
	BacktrackGuard guard(pos);
	if(parse_term(pos, end, env))
	{
		for(;;)
		{
			if(ignore(pos, end, "+"))
			{
				env.out << "push %eax" << endl;
				if(!parse_term(pos, end, env))
				{
					cerr << "Expected term after operator" << endl;
					return false;
				}
				env.out << "addl (%esp), %eax" << endl << "addl $4, %esp" << endl;
			}
			else if(ignore(pos, end, "-"))
			{
				env.out << "push %eax" << endl;
				if(!parse_term(pos, end, env))
				{
					cerr << "Expected term after operator" << endl;
					return false;
				}
				env.out << "subl %eax, (%esp)" << endl << "pop %eax" << endl;
			}
			else
			{
				break;
			}
		}
		guard.no_backtrack();
		return true;
	}
	else
	{
		return false;
	}
}

bool parse_command(const char* &pos, const char* end, ostream &out)
{
	BacktrackGuard guard(pos);
	if(ignore(pos, end, "print"))
	{
		int n;
		if(read_number(pos, end, n))
		{
			out << "pushl $" << n << endl << "call _print_int" << endl << "addl $4, %esp" << endl;
			guard.no_backtrack();
			return true;
		}
		else
		{
			cerr << "Argument to print is missing";
		}
	}
	return false;
}

bool parse_block(Env &env, const char* &pos, const char* end)
{
	BacktrackGuard guard(pos);
	if(ignore(pos, end, "{"))
	{
		env.var.push_scope();
		while(parse_block(env, pos, end))
		{
		}
		if(ignore(pos, end, "}"))
		{
			env.var.pop_scope(env.out);
			guard.no_backtrack();
			return true;
		}
		else
		{
			cerr << "Missing }" << endl;
			return false;
		}
	}
	else
	{
		guard.no_backtrack();
		return parse_command(env, pos, end);
	}
}

bool parse_command(Env &env, const char* &pos, const char* end)
{
	BacktrackGuard guard(pos);
	if(parse_print(env, pos, end))
	{
		guard.no_backtrack();
		return true;
	}
	else if(ignore(pos, end, "var"))
	{
		string identifier;
		if(read_identifier(pos, end, identifier))
		{
			if(env.var.create_var(identifier, env.out))
			{
				guard.no_backtrack();
				return true;
			}
			else
			{
				cerr << "Variable redefined" << endl;
			}
		}
		else
		{
			cerr << "var must be followed by a varaible name" << endl;
			return false;
		}
	}
	else
	{
		string identifier;
		if(read_identifier(pos, end, identifier))
		{
			if(ignore(pos, end, "="))
			{
				if(parse_expression(pos, end, env))
				{
					env.out << "movl %eax, " << env.var.get_var_address(identifier) << endl;
					guard.no_backtrack();
					return true;
				}
				else
				{
					cerr << "Missing right operant of =" << endl;
				}
			}
			else
			{
				cerr << "Missing = in assignment" << endl;
			}
		}
		return false;
	}
}

bool parse_program(Env &env, const char* &pos, const char* end)
{
	return parse_block(env, pos, end);
}

bool parse_print(Env &env, const char* &pos, const char *end)
{
	BacktrackGuard guard(pos);
	if(ignore(pos, end, "print"))
	{
		do
		{
			string str;
			if(read_string(pos, end, str))
			{
				string label = env.label.make_unique();
				env.data.add_string_data(label, str);
				env.out << "pushl $" << label << endl << "call _print_string" << endl << "addl $4, %esp" << endl;
			}
			else if(parse_expression(pos, end, env))
			{
				env.out << "pushl %eax" << endl << "call _print_int" << endl << "addl $4, %esp" << endl;
			}
			else
			{
				cerr << "Argument to print is missing" << endl;
				return false;
			}
		}while(ignore(pos, end, ","));
		guard.no_backtrack();
		return true;
	}
	else
	{
		return false;
	}
}

bool parse_one(const char* &pos, const char* &end, std::ostream&out)
{
	if(ignore(pos, end, "1") || ignore(pos, end, "one"))
	{
		write_print_one_code(out);
		return true;
	}
	else
	{
		cerr << "Unexpected end of file" << endl;
	}
	return false;
}

bool read_identifier(const char* &pos, const char* &end, string &identifier)
{
	skip_spaces(pos, end);
	if(pos != end && (isalpha(*pos) || *pos == "_"))
	{
		identifier = "";
		do
		{
			identifier += *pos;
			++pos;
		}while(pos != end && (isalnum(*pos) || *pos == "_"));
		return true;
	}
	else
	{
		return false;
	}
}

int main(int argc, char* argv[])
{
	ifstream in("in.txt");
	vector<char> source_code;
	copy(istreambuf_iterator<char>(in.rdbuf()), istreambuf_iterator<char>(), back_inserter(source_code));
	ofstream out("out.S");
	out << ".text" << endl << ".globl _prog" << endl << "_prog:" << "push %epb" << "mov %esp, %ebp" << endl;

	const char *pos = &source_code.front();
	const char *end = &source_code.back() + 1;

	VarManager var;
	DataSection data;
	LabelManager label;
	Env env(var, out, data, label);
	if(parse_program(env, pos, end))
	{
		skip_spaces(pos, end);
		if(pos != end)
		{
			cerr << "Unexpected trailing character at end of file" << endl;
		}
		else
		{
			out << "pop %ebp" << endl << "ret" << endl;
		}
	}
}
