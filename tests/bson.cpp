#include "catch2/catch.hpp"
#include <stdio.h>
#include <json.hpp>
#include "util/test_utils.hpp"
#include "util/test_file.hpp"
#include "util/bson/bson.hpp"
#include "util/bson/parser.hpp"
#include <variant>
#include <any>

using namespace nlohmann;
using namespace realm;
using namespace bson;

static inline std::string remove_whitespace(const char* c) {
    std::string str(c);
    str.erase(std::remove_if(str.begin(), str.end(), isspace), str.end());
    return str;
}

/**
 ======== BSON CORPUS ========
 */
template <typename T>
using CorpusCheck = std::function<bool(T)>;

template <typename T>
struct CorpusEntry {
    const char* canonical_extjson;
    CorpusCheck<T> check;
    bool lossy;
};

template <typename T>
static inline void run_corpus(const char* test_key, const CorpusEntry<T>& entry) {
    std::string canonical_extjson = remove_whitespace(entry.canonical_extjson);
    auto val = std::get<BsonDocument>(bson::parse(canonical_extjson));
    auto test_value = val[test_key];
    REQUIRE(std::holds_alternative<T>(test_value));
    CHECK(entry.check(std::get<T>(test_value)));
    if (!entry.lossy) {
        std::stringstream s;
        s << val;
        CHECK(s.str() == canonical_extjson);
    }
}

TEST_CASE("canonical_extjson_corpus", "[bson]") {
    SECTION("Array") {
        SECTION("Empty") {
            run_corpus<BsonArray>("a", {
                "{\"a\" : []}",
                [](auto val) { return val.empty(); }
            });
        }
        SECTION("Single Element Array") {
            run_corpus<BsonArray>("a", {
                "{\"a\" : [{\"$numberInt\": \"10\"}]}",
                [](auto val) { return std::get<int32_t>(val[0]) == 10; }
            });
        }
        SECTION("Multi Element Array") {
            run_corpus<BsonArray>("a", {
                "{\"a\" : [{\"$numberInt\": \"10\"}, {\"$numberInt\": \"20\"}]}",
                [](auto val) { return std::get<int32_t>(val[0]) == 10 && std::get<int32_t>(val[1]) == 20; }
            });
        }
    }

    SECTION("Binary") {
        SECTION("subtype 0x00 (Zero-length)") {
            run_corpus<std::vector<char>>("x", {
                "{\"x\" : { \"$binary\" : {\"base64\" : \"\", \"subType\" : \"00\"}}}",
                [](auto val) { return val == std::vector<char>(); }
            });
        }
        SECTION("subtype 0x00 (Zero-length, keys reversed)") {
            run_corpus<std::vector<char>>("x", {

                "{\"x\" : { \"$binary\" : {\"base64\" : \"\", \"subType\" : \"00\"}}}",
                [](auto val) { return val == std::vector<char>(); }
            });
        }

        SECTION("subtype 0x00") {
            run_corpus<std::vector<char>>("x", {
                "{\"x\" : { \"$binary\" : {\"base64\" : \"//8=\", \"subType\" : \"00\"}}}",
                [](auto val) {
                    std::string bin = "//8=";
                    return val == std::vector<char>(bin.begin(), bin.end());
                }
            });
        }
    }

    SECTION("Boolean") {
        SECTION("True") {
            run_corpus<bool>("b", {
                "{\"b\" : true}",
                [](auto val) { return val; }
            });
        }

        SECTION("False") {
            run_corpus<bool>("b", {
                "{\"b\" : false}",
                [](auto val) { return !val; }
            });
        }
    }

    SECTION("DateTime") {
        SECTION("epoch") {
            run_corpus<time_t>("a", {
                "{\"a\" : {\"$date\" : {\"$numberLong\" : \"0\"}}}",
                [](auto val) { return val == 0; }
            });
        }
        SECTION("positive ms") {
            run_corpus<time_t>("a", {
                "{\"a\" : {\"$date\" : {\"$numberLong\" : \"1356351330501\"}}}",
                [](auto val) { return val == 1356351330501; }
            });
        }
        SECTION("negative") {
            run_corpus<time_t>("a", {
                "{\"a\" : {\"$date\" : {\"$numberLong\" : \"-284643869501\"}}}",
                [](auto val) { return val == -284643869501;
                }
            });
        }
        SECTION("Y10K") {
            run_corpus<time_t>("a", {
                "{\"a\":{\"$date\":{\"$numberLong\":\"253402300800000\"}}}",
                [](auto val) { return val == 253402300800000; }
            });
        };
    }

    SECTION("Decimal") {
        SECTION("Special - Canonical NaN") {
            run_corpus<Decimal128>("d", {
                "{\"d\" : {\"$numberDecimal\" : \"NaN\"}}",
                [](auto val) { return val.is_nan();  }
            });
        }

        SECTION("Special - Canonical Positive Infinity") {
            run_corpus<Decimal128>("d", {
                "{\"d\" : {\"$numberDecimal\" : \"Infinity\"}}",
                [](auto val) { return val == Decimal128("Infinity"); }
            });
        }
        SECTION("Special - Canonical Negative Infinity") {
            run_corpus<Decimal128>("d", {
                "{\"d\" : {\"$numberDecimal\" : \"-Infinity\"}}",
                [](auto val) { return val == Decimal128("-Infinity"); }
            });
        }
        SECTION("Regular - Smallest") {
            run_corpus<Decimal128>("d", {
                "{\"d\" : {\"$numberDecimal\" : \"1.234E-3\"}}",
                [](auto val) { return val == Decimal128("0.001234"); }
            });
        }

        SECTION("Regular - 0.1") {
            run_corpus<Decimal128>("d", {
                "{\"d\" : {\"$numberDecimal\" : \"1E-1\"}}",
                [](auto val) { return val == Decimal128("0.1"); }
            });
        };
    }

    SECTION("Document") {
        SECTION("Empty subdoc") {
            run_corpus<BsonDocument>("x", {
                "{\"x\" : {}}",
                [](auto val) { return val.empty(); }
            });
        }
        SECTION("Empty-string key subdoc") {
            run_corpus<BsonDocument>("x", {
                "{\"x\" : {\"\" : \"b\"}}",
                [](auto val) { return std::get<std::string>(val[""]) == "b"; }
            });
        }
        SECTION("Single-character key subdoc") {
            run_corpus<BsonDocument>("x", {
                "{\"x\" : {\"a\" : \"b\"}}",
                [](auto val) { return std::get<std::string>(val["a"]) == "b"; }
            });
        }
    }

    SECTION("Double type") {
        static float epsilon = 0.000000001;

        SECTION("+1.0") {
            run_corpus<double>("d", {
                "{\"d\" : {\"$numberDouble\": \"1\"}}",
                [](auto val) { return abs(val - 1.0) < epsilon; }
            });
        }
        SECTION("-1.0") {
            run_corpus<double>("d", {
                "{\"d\" : {\"$numberDouble\": \"-1\"}}",
                [](auto val) { return abs(val - -1.0) < epsilon; }
            });
        }
        SECTION("+1.0001220703125") {
            run_corpus<double>("d", {
                "{\"d\" : {\"$numberDouble\": \"1.0001220703125\"}}",
                [](auto val) { return abs(val - 1.0001220703125) < epsilon; },
                true
            });
        }
        SECTION("-1.0001220703125") {
            run_corpus<double>("d", {
                "{\"d\" : {\"$numberDouble\": \"-1.0001220703125\"}}",
                [](auto val) { return abs(val - -1.0001220703125) < epsilon; },
                true
            });
        }
        SECTION("1.2345678921232E+18") {
            run_corpus<double>("d", {
                "{\"d\" : {\"$numberDouble\": \"1.2345678921232E+18\"}}",
                [](auto val) { return abs(val - 1.2345678921232E+18) < epsilon; },
                true
            });
        }
        SECTION("-1.2345678921232E+18") {
            run_corpus<double>("d", {
                "{\"d\" : {\"$numberDouble\": \"-1.2345678921232E+18\"}}",
                [](auto val) { return abs(val - -1.2345678921232E+18) < epsilon; },
                true
            });
        }
        SECTION("0.0") {
            run_corpus<double>("d", {
                "{\"d\" : {\"$numberDouble\": \"0\"}}",
                [](auto val) { return abs(val - 0.0) < epsilon; }

            });
        }
        SECTION("-0.0") {
            run_corpus<double>("d", {
                "{\"d\" : {\"$numberDouble\": \"-0\"}}",
                [](auto val) { return abs(val - -0.0) < epsilon; }
            });
        }
        SECTION("NaN") {
            run_corpus<double>("d", {
                "{\"d\": {\"$numberDouble\": \"NaN\"}}",
                [](auto val) { return isnan(val); }
            });
        }
        SECTION("Inf") {
            run_corpus<double>("d", {
                "{\"d\": {\"$numberDouble\": \"Infinity\"}}",
                [](auto val) { return val == std::numeric_limits<double>::infinity(); }
            });
        }
        SECTION("-Inf") {
            run_corpus<double>("d", {
                "{\"d\": {\"$numberDouble\": \"-Infinity\"}}",
                [](auto val) { return val == (-1 * std::numeric_limits<double>::infinity()); }
            });
        }
    }

    SECTION("Int32 type") {
        SECTION("MinValue") {
            run_corpus<int32_t>("i", {
                "{\"i\" : {\"$numberInt\": \"-2147483648\"}}",
                [](auto val) { return val == -2147483648; }
            });
        }
        SECTION("MaxValue") {
            run_corpus<int32_t>("i", {
                "{\"i\" : {\"$numberInt\": \"2147483647\"}}",
                [](auto val) { return val == 2147483647; }
            });
        }
        SECTION("-1") {
            run_corpus<int32_t>("i", {
                "{\"i\" : {\"$numberInt\": \"-1\"}}",
                [](auto val) { return val == -1; }
            });
        }
        SECTION("0") {
            run_corpus<int32_t>("i", {
                "{\"i\" : {\"$numberInt\": \"0\"}}",
                [](auto val) { return val == 0; }
            });
        }
        SECTION("1") {
            run_corpus<int32_t>("i", {
                "{\"i\" : {\"$numberInt\": \"1\"}}",
                [](auto val) { return val == 1; }
            });
        }
    }

    SECTION("Int64 type") {
        SECTION("MinValue") {
            run_corpus<int64_t>("a", {
                "{\"a\" : {\"$numberLong\" : \"-9223372036854775808\"}}",
                [](auto val) { return val == LLONG_MIN; }
            });
        }
        SECTION("MaxValue") {
            run_corpus<int64_t>("a", {
                "{\"a\" : {\"$numberLong\" : \"9223372036854775807\"}}",
                [](auto val) { return val == LLONG_MAX; }
            });
        }
        SECTION("-1") {
            run_corpus<int64_t>("a", {
                "{\"a\" : {\"$numberLong\" : \"-1\"}}",
                [](auto val) { return val == -1; }
            });
        }
        SECTION("0") {
            run_corpus<int64_t>("a", {
                "{\"a\" : {\"$numberLong\" : \"0\"}}",
                [](auto val) { return val == 0; }
            });
        }
        SECTION("1") {
            run_corpus<int64_t>("a", {
                "{\"a\" : {\"$numberLong\" : \"1\"}}",
                [](auto val) { return val == 1; }
            });
        }
    }

    SECTION("Maxkey type") {
        run_corpus<MaxKey>("a", {
                "{\"a\" : {\"$maxKey\" : 1}}",
                [](auto val) { return val == max_key; }
        });
    }

    SECTION("Minkey type") {
        run_corpus<MinKey>("a", {
                "{\"a\" : {\"$minKey\" : 1}}",
                [](auto val) { return val == min_key; }
        });
    }

    SECTION("Multiple types within the same documment") {
        auto canonical_extjson = remove_whitespace("{\"_id\": {\"$oid\": \"57e193d7a9cc81b4027498b5\"}, \"String\": \"string\", \"Int32\": {\"$numberInt\": \"42\"}, \"Int64\": {\"$numberLong\": \"42\"}, \"Double\": {\"$numberDouble\": \"-1\"}, \"Binary\": { \"$binary\" : {\"base64\": \"o0w498Or7cijeBSpkquNtg==\", \"subType\": \"00\"}}, \"BinaryUserDefined\": { \"$binary\" : {\"base64\": \"AQIDBAU=\", \"subType\": \"00\"}}, \"Subdocument\": {\"foo\": \"bar\"}, \"Array\": [{\"$numberInt\": \"1\"}, {\"$numberInt\": \"2\"}, {\"$numberInt\": \"3\"}, {\"$numberInt\": \"4\"}, {\"$numberInt\": \"5\"}], \"Timestamp\": {\"$timestamp\": {\"t\": 42, \"i\": 1}}, \"Regex\": {\"$regularExpression\": {\"pattern\": \"pattern\", \"options\": \"\"}}, \"DatetimeEpoch\": {\"$date\": {\"$numberLong\": \"0\"}}, \"DatetimePositive\": {\"$date\": {\"$numberLong\": \"2147483647\"}}, \"DatetimeNegative\": {\"$date\": {\"$numberLong\": \"-2147483648\"}}, \"True\": true, \"False\": false, \"Minkey\": {\"$minKey\": 1}, \"Maxkey\": {\"$maxKey\": 1}, \"Null\": null}");

        std::string binary = "o0w498Or7cijeBSpkquNtg==";
        std::string binary_user_defined = "AQIDBAU=";

        const BsonDocument document = {
            { "_id", ObjectId("57e193d7a9cc81b4027498b5") },
            { "String", std::string("string") },
            { "Int32", 42 },
            { "Int64", int64_t(42) },
            { "Double", -1.0 },
            { "Binary", std::vector<char>(binary.begin(), binary.end()) },
            { "BinaryUserDefined", std::vector<char>(binary_user_defined.begin(), binary_user_defined.end()) },
            { "Subdocument", BsonDocument {
                {"foo", std::string("bar") }
            }},
            { "Array", BsonArray {1, 2, 3, 4, 5} },
            { "Timestamp", Timestamp(42, 1) },
            { "Regex", RegularExpression("pattern", "") },
            { "DatetimeEpoch", time_t(0) },
            { "DatetimePositive", time_t(2147483647) },
            { "DatetimeNegative", time_t(-2147483648) },
            { "True", true },
            { "False", false },
            { "Minkey", min_key },
            { "Maxkey", max_key },
            { "Null", bson::null }
        };

        CHECK(std::get<BsonDocument>(bson::parse(canonical_extjson)) == document);
        std::stringstream s;
        s << Bson(document);
        CHECK(canonical_extjson == s.str());
    }

    // TODO: Rest of corpus
}

TEST_CASE("extjson_parser", "[bson]") {
    struct TestParser : bson::detail::Parser {
        using instruction = State;

        std::stack<Instruction>& instructions()
        {
            return m_instructions;
        }

        std::stack<BsonContainer>& marks()
        {
            return m_marks;
        }
    };

    auto parser = TestParser();

    SECTION("null") {
        parser.marks().push({});
        parser.instructions().push({TestParser::instruction::JsonKey, "key"});
        parser.null();
        CHECK(parser.instructions().empty());
        CHECK(parser.marks().top().back().first == "key");
        CHECK(std::get<Null>(parser.marks().top().back().second) == bson::null);
    }

    SECTION("boolean") {
        for (auto& b : {true, false}) {
            parser.marks().push({});
            parser.instructions().push({TestParser::instruction::JsonKey, "key"});
            parser.boolean(b);
            CHECK(parser.instructions().empty());
            CHECK(parser.marks().top().back().first == "key");
            CHECK(std::get<bool>(parser.marks().top().back().second) == b);
        }
    }

    SECTION("number_integer") {
        parser.instructions().push({TestParser::instruction::NumberInt, "key"});
        CHECK_THROWS(parser.number_integer(42));
    }

    SECTION("number_unsigned") {
        parser.instructions().push({});
    }

    // TODO: rest of parser methods
}
