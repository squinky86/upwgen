/*
 * upwgen - the Unspeakable PassWord GENerator
 *
 * Copyright © 2021 Jon Hood, http://www.hoodsecurity.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#endif

using namespace std;

// Pointer to memset is volatile so that compiler must de-reference
// the pointer and can't assume that it points to any function in
// particular (such as memset, which it then might further "optimize")
typedef void* (*memset_t)(void*, int, size_t);

static volatile memset_t memset_func = memset;

void cleanse(void* ptr, size_t len) {
	if (ptr) memset_func(ptr, 0, len);
}

// Secure allocator that zero-fills memory on deallocation
template <typename T>
struct SecureAllocator {
	using value_type = T;
	SecureAllocator() noexcept {}
	template <typename U> SecureAllocator(const SecureAllocator<U>&) noexcept {}
	T* allocate(std::size_t n) {
		return static_cast<T*>(std::malloc(n * sizeof(T)));
	}
	void deallocate(T* p, std::size_t n) {
		if (p) {
			cleanse(p, n * sizeof(T));
			std::free(p);
		}
	}
};

using SecureU32String = std::basic_string<char32_t, std::char_traits<char32_t>, SecureAllocator<char32_t>>;
using SecureString = std::basic_string<char, std::char_traits<char>, SecureAllocator<char>>;

void to_utf8(const SecureU32String& input, SecureString& output) {
	output.clear();
	for (char32_t codepoint : input) {
		if (codepoint <= 0x7F) {
			output += static_cast<char>(codepoint);
		} else if (codepoint <= 0x7FF) {
			output += static_cast<char>(0xC0 | (codepoint >> 6));
			output += static_cast<char>(0x80 | (codepoint & 0x3F));
		} else if (codepoint <= 0xFFFF) {
			output += static_cast<char>(0xE0 | (codepoint >> 12));
			output += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
			output += static_cast<char>(0x80 | (codepoint & 0x3F));
		} else if (codepoint <= 0x10FFFF) {
			output += static_cast<char>(0xF0 | (codepoint >> 18));
			output += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
			output += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
			output += static_cast<char>(0x80 | (codepoint & 0x3F));
		}
	}
}

void PrintHelp()
{
	cout << "Unspeakable Password Generator Version 0.0.1." << endl;
	cout << "Usage: upwgen [-scldnexgmoh] [password size] [password quantity]" << endl;
	cout << "\t-s Disable symbols" << endl;
	cout << "\t-c Disable English capital letters" << endl;
	cout << "\t-l Disable English lowercase letters" << endl;
	cout << "\t-d Disable digits/numbers" << endl;
	cout << "\t-n Disable invisible characters" << endl;
	cout << "\t-e Disable emojis" << endl;
	cout << "\t-x Disable extinct language characters" << endl;
	cout << "\t-g Disable gamepieces" << endl;
	cout << "\t-m Disable diacritical marks" << endl;
	cout << "\t-o Disable other languages/scripts (e.g. Braille)" << endl;
	cout << "Default size: 15" << endl;
	cout << "Default quantity: 1" << endl;
}

//using the provided engine, require 1 character fom the provided keyspace2
//If updatekeyspace is true, add the keyspace2 to keyspace.
template<typename URBG>
void Require(URBG &e, SecureU32String *keyspace, vector<SecureU32String> &passwords, int qty, const SecureU32String keyspace2, bool updateKeyspace = true)
{
	uniform_int_distribution<int> u(0, keyspace2.length() - 1);
	for (int i = 0; i < qty; i++)
	{
		passwords[i] += keyspace2[u(e)];
	}
	if (updateKeyspace)
	{
		*keyspace += keyspace2;
		sort(begin(*keyspace), end(*keyspace));
		auto duplicates = unique(begin(*keyspace), end(*keyspace));
		keyspace->erase(duplicates, end(*keyspace));
	}
}

int main(int argc, char *argv[])
{
	//By default, require everything.
	bool requireSymbol = true;
	bool requireCap = true;
	bool requireLower = true;
	bool requireDigit = true;
	bool requireNonprint = true;
	bool requireEmoji = true;
	bool requireExtinct = true;
	bool requireGame = true;
	bool requireDiacritical = true;
	bool requireOther = true;

	//collection of possible characters to randomly choose from
	SecureU32String keyspace;

	//random number generator
	random_device e;

	int size = -1;
	int qty = -1;

	//parse command line options
	for (int i = 1; i < argc; i++)
	{
		if (strcmp("-s", argv[i]) == 0)
			requireSymbol = false;
		else if (strcmp("-c", argv[i]) == 0)
			requireCap = false;
		else if (strcmp("-l", argv[i]) == 0)
			requireLower = false;
		else if (strcmp("-d", argv[i]) == 0)
			requireDigit = false;
		else if (strcmp("-n", argv[i]) == 0)
			requireNonprint = false;
		else if (strcmp("-e", argv[i]) == 0)
			requireEmoji = false;
		else if (strcmp("-x", argv[i]) == 0)
			requireExtinct = false;
		else if (strcmp("-g", argv[i]) == 0)
			requireGame = false;
		else if (strcmp("-m", argv[i]) == 0)
			requireDiacritical = false;
		else if (strcmp("-o", argv[i]) == 0)
			requireOther = false;
		else if (strcmp("-h", argv[i]) == 0)
		{
			PrintHelp();
			return EXIT_SUCCESS;
		}
		else if (size < 0)
			size = atoi(argv[i]);
		else if (qty < 0)
			qty = atoi(argv[i]);
		else
		{
			PrintHelp();
			return EXIT_FAILURE;
		}
	}

	//set sane defaults
	if (size < 0)
		size = 15;
	if (qty < 0)
		qty = 1;

	if (size == 0)
	{
		cerr << "ERROR: invalid size of password string." << endl;
		PrintHelp();
		return EXIT_FAILURE;
	}
	if (qty == 0)
	{
		cerr << "ERROR: invalid quantity of passwords specified." << endl;
		PrintHelp();
		return EXIT_FAILURE;
	}

	//Use vector instead of VLA
	vector<SecureU32String> passwords(qty);
	
	//require symbols
	if (requireSymbol)
	{
		Require(e, &keyspace, passwords, qty, U"!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~");
	}

	//require capitalization
	if (requireCap)
	{
		Require(e, &keyspace, passwords, qty, U"ABCDEFGHIJKLMNOPQRSTUVWXYZ");
	}

	//require lowercase
	if (requireLower)
	{
		Require(e, &keyspace, passwords, qty, U"abcdefghijklmnopqrstuvwxyz");
	}

	//require digit
	if (requireDigit)
	{
		Require(e, &keyspace, passwords, qty, U"0123456789");
	}

	//require nonprintable
	if (requireNonprint)
	{
		SecureU32String tmpKeyspace;
		for (char32_t i = U'\U000E0020'; i < U'\U000E007F'; i++)
			tmpKeyspace += i;

		Require(e, &keyspace, passwords, qty, tmpKeyspace);
	}

	//require diacritical marks
	if (requireDiacritical)
	{
		SecureU32String tmpKeyspace;
		for (char32_t i = U'\U00000300'; i <= U'\U0000036F'; i++)
			tmpKeyspace += i;

		Require(e, &keyspace, passwords, qty, tmpKeyspace);
	}

	//require emoji
	if (requireEmoji)
	{
		SecureU32String tmpKeyspace;
		for (char32_t i = U'\U0001F600'; i <= U'\U0001F64F'; i++)
			tmpKeyspace += i;

		Require(e, &keyspace, passwords, qty, tmpKeyspace);
	}

	//require an extinct language
	if (requireExtinct)
	{
		SecureU32String tmpKeyspace;
		//Gothic
		for (char32_t i = U'\U00010330'; i <= U'\U0001034A'; i++)
			tmpKeyspace += i;
		//Ugaritic
		for (char32_t i = U'\U00010380'; i <= U'\U0001039D'; i++)
			tmpKeyspace += i;
		//Avestan
		for (char32_t i = U'\U00010B00'; i <= U'\U00010B35'; i++)
			tmpKeyspace += i;
		//Parthian/Pahlavi
		for (char32_t i = U'\U00010B40'; i <= U'\U00010B55'; i++)
			tmpKeyspace += i;
		for (char32_t i = U'\U00010B60'; i <= U'\U00010B72'; i++)
			tmpKeyspace += i;
		for (char32_t i = U'\U00010B80'; i <= U'\U00010B91'; i++)
			tmpKeyspace += i;
		//Cuneiform
		for (char32_t i = U'\U00012000'; i <= U'\U000120FF'; i++)
			tmpKeyspace += i;
		//Hieroglyphics
		for (char32_t i = U'\U00013000'; i <= U'\U000130FF'; i++)
			tmpKeyspace += i;
		//Vithkuqi
		for (char32_t i = U'\U00010570'; i <= U'\U0001057A'; i++)
			tmpKeyspace += i;
		for (char32_t i = U'\U0001057C'; i <= U'\U0001058A'; i++)
			tmpKeyspace += i;
		for (char32_t i = U'\U0001058C'; i <= U'\U00010592'; i++)
			tmpKeyspace += i;
		for (char32_t i = U'\U00010594'; i <= U'\U00010595'; i++)
			tmpKeyspace += i;
		for (char32_t i = U'\U00010597'; i <= U'\U000105A1'; i++)
			tmpKeyspace += i;
		for (char32_t i = U'\U000105A3'; i <= U'\U000105B1'; i++)
			tmpKeyspace += i;
		for (char32_t i = U'\U000105B3'; i <= U'\U000105B9'; i++)
			tmpKeyspace += i;
		for (char32_t i = U'\U000105BB'; i <= U'\U000105BC'; i++)
			tmpKeyspace += i;
		//Cypro-Minoan
		for (char32_t i = U'\U00012F90'; i <= U'\U00012FF2'; i++)
			tmpKeyspace += i;
		//Runic
		for (char32_t i = U'\U000016A0'; i <= U'\U000016FF'; i++)
			tmpKeyspace += i;
		//Ogham
		for (char32_t i = U'\U00001680'; i <= U'\U0000169F'; i++)
			tmpKeyspace += i;
		//Linear B
		for (char32_t i = U'\U00010000'; i <= U'\U0001007F'; i++)
			tmpKeyspace += i;

		Require(e, &keyspace, passwords, qty, tmpKeyspace);
	}

	//require other scripts
	if (requireOther)
	{
		SecureU32String tmpKeyspace;
		//Braille
		for (char32_t i = U'\U00002800'; i <= U'\U000028FF'; i++)
			tmpKeyspace += i;
			
		Require(e, &keyspace, passwords, qty, tmpKeyspace);
	}

	//require a gamepiece
	if (requireGame)
	{
		SecureU32String tmpKeyspace;
		//chess pieces
		for (char32_t i = U'\u2654'; i <= U'\u265F'; i++)
			tmpKeyspace += i;

		//chess symbols
		for (char32_t i = U'\U0001FA00'; i <= U'\U0001FA53'; i++)
			tmpKeyspace += i;

		//Mahjong
		for (char32_t i = U'\U0001F000'; i <= U'\U0001F02B'; i++)
			tmpKeyspace += i;

		//domino
		for (char32_t i = U'\U0001F030'; i <= U'\U0001F093'; i++)
			tmpKeyspace += i;

		//cards
		for (char32_t i = U'\U0001F0A0'; i <= U'\U0001F0AE'; i++)
			tmpKeyspace += i;
		for (char32_t i = U'\U0001F0B1'; i <= U'\U0001F0BF'; i++)
			tmpKeyspace += i;
		for (char32_t i = U'\U0001F0C1'; i <= U'\U0001F0CF'; i++)
			tmpKeyspace += i;
		for (char32_t i = U'\U0001F0D1'; i <= U'\U0001F0F5'; i++)
			tmpKeyspace += i;

		//Xiangqi
		for (char32_t i = U'\U0001FA60'; i <= U'\U0001FA6D'; i++)
			tmpKeyspace += i;

		Require(e, &keyspace, passwords, qty, tmpKeyspace);
	}

	if (keyspace.empty())
	{
		cerr << "ERROR: All character sets disabled. No password can be generated." << endl;
		return EXIT_FAILURE;
	}

	while (passwords[0].length() < static_cast<unsigned long>(size))
		Require(e, &keyspace, passwords, qty, keyspace, false);

	//shuffle passwords to randomize character order
	for (int i = 0; i < qty; i++)
	{
		shuffle(begin(passwords[i]), end(passwords[i]), e);
	}

	//print passwords
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
	setvbuf(stdout, nullptr, _IOFBF, 1000);
#endif
	for (int i = 0; i < qty; i++)
	{
		SecureString utf8_pw;
		to_utf8(passwords[i], utf8_pw);
		cout << i+1 << ": |" << utf8_pw << "|" << endl;
	}

	return EXIT_SUCCESS;
}
