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
#include <codecvt>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <locale>
#include <random>

using namespace std;

//pulled from https://stackoverflow.com/questions/5698002/how-does-one-securely-clear-stdstring/59179980#59179980
// Pointer to memset is volatile so that compiler must de-reference
// the pointer and can't assume that it points to any function in
// particular (such as memset, which it then might further "optimize")
typedef void* (*memset_t)(void*, int, size_t);

static volatile memset_t memset_func = memset;

void cleanse(void* ptr, size_t len) {
	memset_func(ptr, 0, len);
}

//codecvt destructor is not public in GCC; template out
template<class I, class E, class S>
struct codecvt2 : std::codecvt<I, E, S>
{
	~codecvt2()
	{ }
};

void PrintHelp()
{
	cout << "Unspeakable Password Generator Version 0.0.1." << endl;
	cout << "Usage: upwgen [-sclnefgh] [password size] [password quantity]" << endl;
	cout << "\t-s Exclude at least 1 symbol" << endl;
	cout << "\t-c Exclude at least 1 English capital letter" << endl;
	cout << "\t-l Exclude at least 1 English lowercase letter" << endl;
	cout << "\t-d Exclude at least 1 digit/number" << endl;
	cout << "\t-n Exclude at least 1 invisible character" << endl;
	cout << "\t-e Exclude at least 1 emoji" << endl;
	cout << "\t-f Exclude at least 1 extinct language character" << endl;
	cout << "\t-f Exclude at least 1 gamepiece" << endl;
	cout << "Default size: 15" << endl;
	cout << "Default quantity: 1" << endl;
}

//using the provided engine, require 1 character fom the provided keyspace2
//If updatekeyspace is true, add the keyspace2 to keyspace.
void Require(default_random_engine *e, u32string *keyspace, u32string passwords[], int qty, const u32string keyspace2, bool updateKeyspace = true)
{
	uniform_int_distribution<int> u(0, keyspace2.length() - 1);
	for (int i = 0; i < qty; i++)
	{
		passwords[i] += keyspace2[u(*e)];
	}
	if (updateKeyspace)
	{
		*keyspace += keyspace2;
		sort(begin(*keyspace), end(*keyspace));
		auto duplicates = unique(begin(*keyspace), end(*keyspace));
		*keyspace = u32string(begin(*keyspace), duplicates);
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

	//collection of possible characters to randomly choose from
	u32string keyspace;

	//random number generator
	random_device r;
	default_random_engine e(r());

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
		else if (strcmp("-f", argv[i]) == 0)
			requireExtinct = false;
		else if (strcmp("-g", argv[i]) == 0)
			requireGame = false;
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
	if (size < 1)
		size = 15;
	if (qty < 1)
		qty = 1;

	bool err = false;
	if (size == 0)
	{
		cerr << "ERROR: invalid size of password string." << endl;
		err = true;
	}
	if (qty == 0)
	{
		cerr << "ERROR: invalid quantity of passwords specified." << endl;
		err = true;
	}
	if (err)
	{
		PrintHelp();
		return EXIT_FAILURE;
	}

	//declare volatile to zero-out memory at the end
	u32string passwords[qty];
	
	//require symbols
	if (requireSymbol)
	{
		Require(&e, &keyspace, passwords, qty, U"!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~");
	}

	//require capitalization
	if (requireCap)
	{
		Require(&e, &keyspace, passwords, qty, U"ABCDEFGHIJKLMNOPQRSTUVWXYZ");
	}

	//require lowercase
	if (requireLower)
	{
		Require(&e, &keyspace, passwords, qty, U"abcdefghijklmnopqrstuvwxyz");
	}

	//require digit
	if (requireDigit)
	{
		Require(&e, &keyspace, passwords, qty, U"0123456789");
	}

	//require nonprintable
	if (requireNonprint)
	{
		u32string tmpKeyspace;
		for (char32_t i = U'\U000E0020'; i < U'\U000E007F'; i++)
			tmpKeyspace += i;

		Require(&e, &keyspace, passwords, qty, tmpKeyspace);
	}

	//require emoji
	if (requireEmoji)
	{
		u32string tmpKeyspace;
		for (char32_t i = U'\U0001F600'; i <= U'\U0001F64F'; i++)
			tmpKeyspace += i;

		Require(&e, &keyspace, passwords, qty, tmpKeyspace);
	}

	//require an extinct language
	if (requireExtinct)
	{
		u32string tmpKeyspace;
		//choose a random foreign language
		uniform_int_distribution<int> u(0, 7);
		switch (u(e))
		{
			case 0: //Gothic
				for (char32_t i = U'\U00010330'; i <= U'\U0001034A'; i++)
					tmpKeyspace += i;
				break;

			case 1: //Ugaritic
				for (char32_t i = U'\U00010380'; i <= U'\U0001039D'; i++)
					tmpKeyspace += i;
				break;

			case 2: //Avestan
				for (char32_t i = U'\U00010B00'; i <= U'\U00010B35'; i++)
					tmpKeyspace += i;
				break;

			case 3: //Parthian/Pahlavi
				for (char32_t i = U'\U00010B40'; i <= U'\U00010B55'; i++)
					tmpKeyspace += i;
				for (char32_t i = U'\U00010B60'; i <= U'\U00010B72'; i++)
					tmpKeyspace += i;
				for (char32_t i = U'\U00010B80'; i <= U'\U00010B91'; i++)
					tmpKeyspace += i;
				break;

			case 4: //Cuneiform
				for (char32_t i = U'\U00012000'; i <= U'\U000120FF'; i++)
					tmpKeyspace += i;
				break;

			case 5: //Hieroglyphics
				for (char32_t i = U'\U00013000'; i <= U'\U000130FF'; i++)
					tmpKeyspace += i;
				break;

			case 6: //Vithkuqi
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
				break;

			case 7: //Cypro-Minoan
				for (char32_t i = U'\U00012F90'; i <= U'\U00012FF2'; i++)
					tmpKeyspace += i;
				break;

			default:
				break;
		}

		Require(&e, &keyspace, passwords, qty, tmpKeyspace);
	}

	//require a gamepiece
	if (requireGame)
	{
		u32string tmpKeyspace;
		//chess pieces
		for (char32_t i = u'\u2654'; i <= u'\u265F'; i++)
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

		Require(&e, &keyspace, passwords, qty, tmpKeyspace);
	}

	while (passwords[0].length() < static_cast<unsigned long>(size))
		Require(&e, &keyspace, passwords, qty, keyspace, false);

	//shuffle passwords to randomize character order
	for (int i = 0; i < qty; i++)
	{
		shuffle(begin(passwords[i]), end(passwords[i]), e);
	}

	//print passwords
	wstring_convert<codecvt2<char32_t, char, std::mbstate_t>, char32_t> conv32;
	for (int i = 0; i < qty; i++)
	{
		cout << i+1 << ": |" << conv32.to_bytes(passwords[i]) << "|" << endl;
		//zeroize password
		passwords[i].resize(passwords[i].capacity(), 0);
		cleanse(&passwords[i][0], passwords[i].size());
		passwords[i].clear();
	}

	return EXIT_SUCCESS;
}
