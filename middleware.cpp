#include <iostream>
#include <string>
#include <asio.hpp>
#include <map>
#include <vector>
#include <stdexcept>

using namespace std;
typedef union OutputHeader_u {
	struct {
		uint32_t preamble = 0xFE00;
		uint32_t count;
	};
	uint8_t bytes[8];
} OutputHeader;

typedef struct OutputObject_s {
	union {
		struct {
			int64_t id;
			int32_t x;
			int32_t y;
			uint8_t type;
		};
		uint8_t bytes[17];
	} fields;
	const uint8_t* color;
} OutputObject;

static vector<string> split(string str, string delim) {
	size_t pos_start = 0, pos_end, delim_len = delim.length();
	string token;
	vector<string> res;

	while ((pos_end = str.find(delim, pos_start)) != string::npos) {
		token = str.substr(pos_start, pos_end - pos_start);
		pos_start = pos_end + delim_len;
		res.push_back(token);
	}

	res.push_back(str.substr(pos_start));
	return res;
}

class Timer {
public:
	typedef chrono::milliseconds Interval;
	typedef function<void(void)> Timeout;

	void start(const Interval &interval, const Timeout &timeout) {
		mThread = thread([this, interval, timeout] {
			while (true) {
				this_thread::sleep_for(interval);
				timeout();
			}
		});
	}

private:
	thread mThread{};
};

static const uint8_t red[] = { 0x5B, 0x31, 0x6D };
static const uint8_t yellow[] = { 0x5B, 0x33, 0x6D };
static const uint8_t blue[] = { 0x5B, 0x34, 0x6D };
static const size_t colorLength = sizeof(red) / sizeof(uint8_t);

static const string badlyFormedStrExceptionPrefix = "Badly formed string: ";

static void printException(const exception& e) {
	clog << "Exception caught: " << e.what() << endl;
}

static int64_t parseIdFromServerStr(string str) {
	const char* idHeader = "ID=";
	const int idHeaderLength = strlen(idHeader);
	if (strncmp(str.c_str(), idHeader, idHeaderLength) != 0) {
		throw runtime_error(badlyFormedStrExceptionPrefix + str);
	}
	const string idStr = str.substr(idHeaderLength, str.find(';', idHeaderLength) - idHeaderLength);
	return stol(idStr);
}

static OutputObject parseServerStr(string str, int64_t id) {
	const auto strSplit = split(str, ";");
	if (strSplit.size() != 4) {
		throw runtime_error(badlyFormedStrExceptionPrefix + str);
	}
	const char* xHeader = "X=";
	const char* yHeader = "Y=";
	const char* typeHeader = "TYPE=";
	const size_t xHeaderLength = strlen(xHeader);
	const size_t yHeaderLength = strlen(yHeader);
	const size_t typeHeaderLength = strlen(typeHeader);
	string xStr = strSplit[1];
	string yStr = strSplit[2];
	string typeStr = strSplit[3];
	if (strncmp(xStr.c_str(), xHeader, xHeaderLength) != 0  ||
		strncmp(yStr.c_str(), yHeader, yHeaderLength) != 0 ||
		strncmp(typeStr.c_str(), typeHeader, typeHeaderLength) != 0) {
		throw runtime_error(badlyFormedStrExceptionPrefix + str);
	}
	xStr = xStr.substr(xHeaderLength, xStr.length() - xHeaderLength);
	yStr = yStr.substr(yHeaderLength, yStr.length() - yHeaderLength);
	typeStr = typeStr.substr(typeHeaderLength, typeStr.length() - typeHeaderLength);
	const int32_t x = stoi(xStr);
	const int32_t y = stoi(yStr);
	const uint8_t type = stoi(typeStr);

	const uint32_t xCartesian = 150;
	const uint32_t yCartesian = 150;

	const uint32_t xDistance = x - xCartesian;
	const uint32_t yDistance = y - yCartesian;
	const uint32_t distance = round(sqrt(xDistance * xDistance + yDistance * yDistance));

	OutputObject res { .fields { .id = id, .x = x, .y = y, .type = type } };
	const size_t colorSize = 3;

	if (type == 3) {
		if (distance < 100) {
			res.color = red;
		} else {
			res.color = yellow;
		}
	} else if (type == 1) {
		if (distance < 50) {
			res.color = red;
		} else if (distance < 75) {
			res.color = yellow;
		} else {
			res.color = blue;
		}
	} else if (type == 2) {
		if (distance < 50) {
			res.color = yellow;
		} else {
			res.color = blue;
		}
	} else {
		throw runtime_error("Unrecognized type " + typeStr);
	}

	return res;
}

int main() {
	asio::ip::tcp::iostream input("localhost", "5463");

	map<uint64_t, string> latestServerStrings;

	Timer tm;
	tm.start(chrono::milliseconds(1667), [&latestServerStrings] {
		const OutputHeader header = { .count = static_cast<uint32_t>(latestServerStrings.size()) };
		cout << header.bytes;
		for (const auto& [id, str]: latestServerStrings) {
			try {
				const auto object = parseServerStr(str, id);
				for (size_t i = 0; i < sizeof(object.fields.bytes); i++) {
					cout << object.fields.bytes[i];
				}
				for (size_t i = 0; i < colorLength; i++) {
					cout << object.color[i];
				}
			} catch (const exception& e) {
				printException(e);
			}
		}
		flush(cout);
	});

	for (string str; getline(input, str);) {
		try {
			const int64_t id = parseIdFromServerStr(str);
			latestServerStrings[id] = str;
		} catch (const exception& e) {
			printException(e);
		}
	}
}
