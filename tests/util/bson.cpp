#include "catch2/catch.hpp"
#include <stdio.h>
#include <json.hpp>
#include "util/test_utils.hpp"
#include "util/test_file.hpp"
#include "util/bson.hpp"
#include "util/extended_json.hpp"
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
using corpus_check_t = std::function<bool(T)>;

template <typename T>
struct corpus_entry_t {
    const char* description;
    const char* canonical_extjson;
    corpus_check_t<T> check;
    bool lossy;
};

template <typename T>
using corpus_t = std::vector<corpus_entry_t<T>>;

template <typename T>
static inline void run_corpus(const char* test_key, const corpus_t<T>& corpus) {
    for (auto entry : corpus) {
        SECTION(entry.description) {
            std::string canonical_extjson = remove_whitespace(entry.canonical_extjson);
            auto val = std::get<BsonDocument>(bson::parse(canonical_extjson));
            auto test_value = val[test_key];
            CHECK(std::holds_alternative<T>(test_value));
            CHECK(entry.check(std::get<T>(test_value)));
            if (!entry.lossy) {
                CHECK(bson::to_json(val) == canonical_extjson);
            }
        }
    }
}

TEST_CASE("canonical_extjson_corpus", "[bson]") {
    SECTION("Array") {
        run_corpus<BsonArray>("a", {{
            "Empty",
            "{\"a\" : []}",
            [](auto val) { return val.empty(); }
        },
        {
            "Single Element Array",
            "{\"a\" : [{\"$numberInt\": \"10\"}]}",
            [](auto val) { return val[0] == 10; }
        },
        {
            "Multi Element Array",
            "{\"a\" : [{\"$numberInt\": \"10\"}, {\"$numberInt\": \"20\"}]}",
            [](auto val) { return val[0] == 10 && val[1] == 20; }
        }});
    }

    SECTION("Binary") {
        run_corpus<BinaryData>("x", {
            {
                "subtype 0x00 (Zero-length)",
                "{\"x\" : { \"$binary\" : {\"base64\" : \"\", \"subType\" : \"00\"}}}",
                [](auto val) { return val == BinaryData(); }
            },
            {
                "subtype 0x00 (Zero-length, keys reversed)",
                "{\"x\" : { \"$binary\" : {\"base64\" : \"\", \"subType\" : \"00\"}}}",
                [](auto val) { return val == BinaryData(); }
            },
            {
                "subtype 0x00",
                "{\"x\" : { \"$binary\" : {\"base64\" : \"//8=\", \"subType\" : \"00\"}}}",
                [](auto val) { return val == BinaryData("//8="); }
            },
        });
    }

    SECTION("Boolean") {
        run_corpus<bool>("b", {
            {
                "True",
                "{\"b\" : true}",
                [](auto val) { return val; }
            },
            {
                "False",
                "{\"b\" : false}",
                [](auto val) { return !val; }
            }
        });
    }

    SECTION("DateTime") {
        run_corpus<time_t>("a", {
            {
                "epoch",
                "{\"a\" : {\"$date\" : {\"$numberLong\" : \"0\"}}}",
                [](auto val) { return val == 0; }
            },
            {
                "positive ms",
                "{\"a\" : {\"$date\" : {\"$numberLong\" : \"1356351330501\"}}}",
                [](auto val) { return val == 1356351330501; }
            },
            {
                "negative",
                "{\"a\" : {\"$date\" : {\"$numberLong\" : \"-284643869501\"}}}",
                [](auto val) { return val == -284643869501;
                }
            },
            {
                "Y10K",
                "{\"a\":{\"$date\":{\"$numberLong\":\"253402300800000\"}}}",
                [](auto val) { return val == 253402300800000; }
            },
        });
    }

    SECTION("Decimal") {
        run_corpus<Decimal128>("d", {
            {
                "Special - Canonical NaN",
                "{\"d\" : {\"$numberDecimal\" : \"NaN\"}}",
                [](auto val) { return val.is_nan();  }
            },
            {
                "Special - Canonical Positive Infinity",
                "{\"d\" : {\"$numberDecimal\" : \"Infinity\"}}",
                [](auto val) { return val == Decimal128("Infinity"); }
            },
            {
                "Special - Canonical Negative Infinity",
                "{\"d\" : {\"$numberDecimal\" : \"-Infinity\"}}",
                [](auto val) { return val == Decimal128("-Infinity"); }
            },
            {
                "Regular - Smallest",
                "{\"d\" : {\"$numberDecimal\" : \"1.234E-3\"}}",
                [](auto val) { return val == Decimal128("0.001234"); }
            },
            {
                "Regular - 0.1",
                "{\"d\" : {\"$numberDecimal\" : \"1E-1\"}}",
                [](auto val) { return val == Decimal128("0.1"); }
            },
        });
    }

    SECTION("Document") {
        run_corpus<BsonDocument>("x", {
            {
                "Empty subdoc",
                "{\"x\" : {}}",
                [](auto val) { return val.empty(); }
            },
            {
                "Empty-string key subdoc",
                "{\"x\" : {\"\" : \"b\"}}",
                [](auto val) { return val[""] == "b"; }
            },
            {
                "Single-character key subdoc",
                "{\"x\" : {\"a\" : \"b\"}}",
                [](auto val) { return val["a"] == "b"; }
            }
        });
    }

    SECTION("Double type") {
        static float epsilon = 0.000000001;
        run_corpus<double>("d", {
            {
                "+1.0",
                "{\"d\" : {\"$numberDouble\": \"1\"}}",
                [](auto val) { return abs(val - 1.0) < epsilon; }
            },
            {
                "-1.0",
                "{\"d\" : {\"$numberDouble\": \"-1\"}}",
                [](auto val) { return abs(val - -1.0) < epsilon; }
            },
            {
                "+1.0001220703125",
                "{\"d\" : {\"$numberDouble\": \"1.0001220703125\"}}",
                [](auto val) { return abs(val - 1.0001220703125) < epsilon; },
                true
            },
            {
                "-1.0001220703125",
                "{\"d\" : {\"$numberDouble\": \"-1.0001220703125\"}}",
                [](auto val) { return abs(val - -1.0001220703125) < epsilon; },
                true
            },
            {
                "1.2345678921232E+18",
                "{\"d\" : {\"$numberDouble\": \"1.2345678921232E+18\"}}",
                [](auto val) { return abs(val - 1.2345678921232E+18) < epsilon; },
                true
            },
            {
                "-1.2345678921232E+18",
                "{\"d\" : {\"$numberDouble\": \"-1.2345678921232E+18\"}}",
                [](auto val) { return abs(val - -1.2345678921232E+18) < epsilon; },
                true
            },
            {
                "0.0",
                "{\"d\" : {\"$numberDouble\": \"0\"}}",
                [](auto val) { return abs(val - 0.0) < epsilon; }

            },
            {
                "-0.0",
                "{\"d\" : {\"$numberDouble\": \"-0\"}}",
                [](auto val) { return abs(val - -0.0) < epsilon; }
            },
            {
                "NaN",
                "{\"d\": {\"$numberDouble\": \"NaN\"}}",
                [](auto val) { return isnan(val); }
            },
            {
                "Inf",
                "{\"d\": {\"$numberDouble\": \"Infinity\"}}",
                [](auto val) { return val == std::numeric_limits<double>::infinity(); }
            },
            {
                "-Inf",
                "{\"d\": {\"$numberDouble\": \"-Infinity\"}}",
                [](auto val) { return val == (-1 * std::numeric_limits<double>::infinity()); }
            }
        });
    }

    SECTION("Int32 type") {
        run_corpus<int32_t>("i", {
            {
                "MinValue",
                "{\"i\" : {\"$numberInt\": \"-2147483648\"}}",
                [](auto val) { return val == -2147483648; }
            },
            {
                "MaxValue",
                "{\"i\" : {\"$numberInt\": \"2147483647\"}}",
                [](auto val) { return val == 2147483647; }
            },
            {
                "-1",
                "{\"i\" : {\"$numberInt\": \"-1\"}}",
                [](auto val) { return val == -1; }
            },
            {
                "0",
                "{\"i\" : {\"$numberInt\": \"0\"}}",
                [](auto val) { return val == 0; }
            },
            {
                "1",
                "{\"i\" : {\"$numberInt\": \"1\"}}",
                [](auto val) { return val == 1; }
            }
        });
    }

    SECTION("Int64 type") {
        run_corpus<int64_t>("a", {
            {
                "MinValue",
                "{\"a\" : {\"$numberLong\" : \"-9223372036854775808\"}}",
                [](auto val) { return val == -9223372036854775808; }
            },
            {
                "MaxValue",
                "{\"a\" : {\"$numberLong\" : \"9223372036854775807\"}}",
                [](auto val) { return val == 9223372036854775807; }
            },
            {
                "-1",
                "{\"a\" : {\"$numberLong\" : \"-1\"}}",
                [](auto val) { return val == -1; }
            },
            {
                "0",
                "{\"a\" : {\"$numberLong\" : \"0\"}}",
                [](auto val) { return val == 0; }
            },
            {
                "1",
                "{\"a\" : {\"$numberLong\" : \"1\"}}",
                [](auto val) { return val == 1; }
            }
        });
    }

    SECTION("Maxkey type") {
        run_corpus<MaxKey>("a", {
            {
                "Maxkey",
                "{\"a\" : {\"$maxKey\" : 1}}",
                [](auto val) { return val == max_key; }
            }
        });
    }

    SECTION("Minkey type") {
        run_corpus<MinKey>("a", {
            {
                "Minkey",
                "{\"a\" : {\"$minKey\" : 1}}",
                [](auto val) { return val == min_key; }
            }
        });
    }

    SECTION("Multiple types within the same documment") {
        auto canonical_extjson = remove_whitespace("{\"_id\": {\"$oid\": \"57e193d7a9cc81b4027498b5\"}, \"String\": \"string\", \"Int32\": {\"$numberInt\": \"42\"}, \"Int64\": {\"$numberLong\": \"42\"}, \"Double\": {\"$numberDouble\": \"-1\"}, \"Binary\": { \"$binary\" : {\"base64\": \"o0w498Or7cijeBSpkquNtg==\", \"subType\": \"00\"}}, \"BinaryUserDefined\": { \"$binary\" : {\"base64\": \"AQIDBAU=\", \"subType\": \"00\"}}, \"Subdocument\": {\"foo\": \"bar\"}, \"Array\": [{\"$numberInt\": \"1\"}, {\"$numberInt\": \"2\"}, {\"$numberInt\": \"3\"}, {\"$numberInt\": \"4\"}, {\"$numberInt\": \"5\"}], \"Timestamp\": {\"$timestamp\": {\"t\": 42, \"i\": 1}}, \"Regex\": {\"$regularExpression\": {\"pattern\": \"pattern\", \"options\": \"\"}}, \"DatetimeEpoch\": {\"$date\": {\"$numberLong\": \"0\"}}, \"DatetimePositive\": {\"$date\": {\"$numberLong\": \"2147483647\"}}, \"DatetimeNegative\": {\"$date\": {\"$numberLong\": \"-2147483648\"}}, \"True\": true, \"False\": false, \"Minkey\": {\"$minKey\": 1}, \"Maxkey\": {\"$maxKey\": 1}, \"Null\": null}");

        BsonDocument document = {
            { "_id", ObjectId("57e193d7a9cc81b4027498b5") },
            { "String", "string" },
            { "Int32", 42 },
            { "Int64", int64_t(42) },
            { "Double", -1.0 },
            { "Binary", BinaryData("o0w498Or7cijeBSpkquNtg==") },
            { "BinaryUserDefined", BinaryData("AQIDBAU=") },
            { "Subdocument", BsonDocument {
                {"foo", "bar"}
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
            { "Null", bson_null }
        };

        CHECK(bson::parse(canonical_extjson) == document);
        CHECK(canonical_extjson == bson::to_json(document));
    }

    // TODO: Rest of corpus
}

TEST_CASE("extjson_parser", "[bson]") {
    struct TestParser : bson::detail::parser {
        using instruction = state_t;

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
        parser.instructions().push({TestParser::instruction::json_key, "key"});
        parser.null();
        CHECK(parser.instructions().empty());
        CHECK(parser.marks().top().back().first == "key");
        CHECK(std::get<bson_null_t>(parser.marks().top().back().second) == bson_null_t());
    }

    SECTION("boolean") {
        for (auto& b : {true, false}) {
            parser.marks().push({});
            parser.instructions().push({TestParser::instruction::json_key, "key"});
            parser.boolean(b);
            CHECK(parser.instructions().empty());
            CHECK(parser.marks().top().back().first == "key");
            CHECK(std::get<bool>(parser.marks().top().back().second) == b);
        }
    }

    SECTION("number_integer") {
        parser.instructions().push({TestParser::instruction::number_int, "key"});
        CHECK_THROWS(parser.number_integer(42));
    }

    SECTION("number_unsigned") {
        parser.instructions().push({});
    }

    // TODO: rest of parser methods
}
