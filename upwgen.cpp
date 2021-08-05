#include <algorithm>
#include <codecvt>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <locale>
#include <random>

using namespace std;

template<class I, class E, class S>
struct codecvt2 : std::codecvt<I, E, S>
{
	~codecvt2()
	{ }
};

void PrintHelp()
{
	cout << "Unspeakable Password Generator Version 0.0.1." << endl;
	cout << "Usage: upwgen [-sclnefh] [password size] [password quantity]" << endl;
	cout << "\t-s Include at least 1 symbol" << endl;
	cout << "\t-c Include at least 1 English capital letter" << endl;
	cout << "\t-l Include at least 1 English lowercase letter" << endl;
	cout << "\t-n Include at least 1 invisible character" << endl;
	cout << "\t-e Include at least 1 emoji" << endl;
	cout << "\t-f Include at least 1 foreign language character" << endl;
	cout << "Default size: 17" << endl;
	cout << "Default quantity: 1" << endl;
}

void Require(default_random_engine *e, u32string *keyspace, u32string passwords[], int qty, const u32string keyspace2, bool updateKeyspace = true)
{
	uniform_int_distribution<int> u(0, keyspace2.length() - 1);
	for (int i = 0; i < qty; i++)
	{
		passwords[i] += keyspace2[u(*e)];
	}
	if (updateKeyspace)
		*keyspace += keyspace2;
}

int main(int argc, char *argv[])
{
	//By default, require everything.
	bool requireSymbol = true;
	bool requireCap = true;
	bool requireLower = true;
	bool requireNonprint = true;
	bool requireEmoji = true;
	bool requireForeign = true;

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
		else if (strcmp("-n", argv[i]) == 0)
			requireNonprint = false;
		else if (strcmp("-e", argv[i]) == 0)
			requireEmoji = false;
		else if (strcmp("-f", argv[i]) == 0)
			requireForeign = false;
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
		size = 17;
	if (qty < 0)
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

	//require nonprintable
	if (requireNonprint)
	{
		u32string tmpKeyspace;
		for (char32_t i = U'\U000E0020'; i < U'\U000E007F'; i++)
			tmpKeyspace += i;

		Require(&e, &keyspace, passwords, qty, tmpKeyspace);
	}
	if (requireEmoji)
	{
		u32string tmpKeyspace;
		for (char32_t i = U'\U0001F600'; i <= U'\U0001F64F'; i++)
			tmpKeyspace += i;

		Require(&e, &keyspace, passwords, qty, tmpKeyspace);
	}
	if (requireForeign)
	{
		u32string tmpKeyspace;
		//choose a random foreign language
		uniform_int_distribution<int> u(0, 5);
		switch (u(e))
		{
			case 0: //Greek and Cyrillic
				for (char32_t i = u'\u038E'; i <= u'\u03A1'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u03A3'; i <= u'\u046F'; i++)
					tmpKeyspace += i;
				break;
			case 1: //Armenian, Hebrew, and Arabic
				for (char32_t i = u'\u0531'; i <= u'\u0556'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u0559'; i <= u'\u058A'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u05D0'; i <= u'\u05EA'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u0600'; i <= u'\u061C'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u061E'; i <= u'\u062F'; i++)
					tmpKeyspace += i;
				break;
			case 2: //Nko, Samaritan, Mandaic, Syriac, and Arabic
				for (char32_t i = u'\u07C0'; i <= u'\u07FA'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u07FD'; i <= u'\u082D'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u0830'; i <= u'\u083E'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u0840'; i <= u'\u085B'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u0860'; i <= u'\u086A'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u08A0'; i <= u'\u08B4'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u08B6'; i <= u'\u08BF'; i++)
					tmpKeyspace += i;
				break;
			case 3: //Ethiopic
				for (char32_t i = u'\u1200'; i <= u'\u1248'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u124A'; i <= u'\u124D'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u1250'; i <= u'\u1256'; i++)
					tmpKeyspace += i;
				tmpKeyspace += u'\u1258';
				for (char32_t i = u'\u125A'; i <= u'\u125D'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u1260'; i <= u'\u1288'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u128A'; i <= u'\u128D'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u1290'; i <= u'\u12B0'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u12D8'; i <= u'\u12FF'; i++)
					tmpKeyspace += i;
				break;
			case 4: //Cherokee and Canadian Aboriginal
				for (char32_t i = u'\u13A0'; i <= u'\u13F5'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u13F8'; i <= u'\u13FD'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u1400'; i <= u'\u149F'; i++)
					tmpKeyspace += i;
				break;
			case 5: //Runic, Tagaalog, Hanunoo, Buhid, Tagbanwa, and Khmer
				for (char32_t i = u'\u16A0'; i <= u'\u16F8'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u1700'; i <= u'\u170C'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u170E'; i <= u'\u1714'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u1720'; i <= u'\u1736'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u1740'; i <= u'\u1753'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u1760'; i <= u'\u176C'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u176E'; i <= u'\u1770'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u1772'; i <= u'\u1773'; i++)
					tmpKeyspace += i;
				for (char32_t i = u'\u1780'; i <= u'\u179F'; i++)
					tmpKeyspace += i;
				break;
			default:
				break;
		}

		Require(&e, &keyspace, passwords, qty, tmpKeyspace);
	}

	while (passwords[0].length() < size)
		Require(&e, &keyspace, passwords, qty, keyspace, false);

	//shuffle passwords
	for (int i = 0; i < qty; i++)
	{
		shuffle(begin(passwords[i]), end(passwords[i]), e);
	}

	//print passwords
	wstring_convert<codecvt2<char32_t, char, std::mbstate_t>, char32_t> conv32;
	for (int i = 0; i < qty; i++)
	{
		cout << i+1 << ": |" << conv32.to_bytes(passwords[i]) << "|" << endl;
	}

	return EXIT_SUCCESS;
}
