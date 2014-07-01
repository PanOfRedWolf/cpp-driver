#define BOOST_TEST_DYN_LINK
#ifdef STAND_ALONE
#   define BOOST_TEST_MODULE cassandra
#endif

#include "cassandra.h"
#include "test_utils.hpp"

#include <algorithm>

#include <boost/test/unit_test.hpp>
#include <boost/test/debug.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/cstdint.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>

struct CollectionsTests : test_utils::MultipleNodesTest {
    CollectionsTests() : MultipleNodesTest(1, 0) {}
};

BOOST_FIXTURE_TEST_SUITE(collections, CollectionsTests)

template<class T>
void insert_collection_value(CassSession* session, CassValueType type, CassValueType primary_type, const std::vector<T>& values) {
  std::string table_name = str(boost::format("%s_%s") % test_utils::SIMPLE_TABLE % test_utils::get_value_type(primary_type));
  std::string type_name = str(boost::format("%s<%s>") % test_utils::get_value_type(type) % test_utils::get_value_type(primary_type));

  test_utils::execute_query(session, str(boost::format("CREATE TABLE %s (tweet_id int PRIMARY KEY, test_val %s);")
                                         % table_name % type_name));

  test_utils::CassCollectionPtr input = test_utils::make_shared(cass_collection_new(static_cast<CassCollectionType>(type), values.size()));

  for(typename std::vector<T>::const_iterator it = values.begin(),
      end = values.end(); it != end; ++it) {
    test_utils::Value<T>::append(input.get(), *it);
  }

  std::string query = str(boost::format("INSERT INTO %s (tweet_id, test_val) VALUES(0, ?);") % table_name);

  test_utils::CassStatementPtr statement = test_utils::make_shared(cass_statement_new(cass_string_init(query.c_str()), 1));

  BOOST_REQUIRE(cass_statement_bind_collection(statement.get(), 0, input.get()) == CASS_OK);

  test_utils::CassFuturePtr result_future = test_utils::make_shared(cass_session_execute(session, statement.get()));
  test_utils::wait_and_check_error(result_future.get());

  test_utils::CassResultPtr result;
  test_utils::execute_query(session, str(boost::format("SELECT * FROM %s WHERE tweet_id = 0;") % table_name), &result);
  BOOST_REQUIRE(cass_result_row_count(result.get()) == 1);
  BOOST_REQUIRE(cass_result_column_count(result.get()) > 0);

  const CassRow* row = cass_result_first_row(result.get());

  const CassValue* output = cass_row_get_column(row, 1);
  BOOST_REQUIRE(cass_value_type(output) == type);
  BOOST_REQUIRE(cass_value_primary_sub_type(output) == primary_type);

  test_utils::CassIteratorPtr iterator = test_utils::make_shared(cass_iterator_from_collection(output));

  if(type == CASS_VALUE_TYPE_LIST) {
    size_t i = 0;
    while(cass_iterator_next(iterator.get())) {
      T result_value;
      BOOST_REQUIRE(test_utils::Value<T>::get(cass_iterator_get_value(iterator.get()), &result_value) == CASS_OK);
      BOOST_REQUIRE(cass_value_type(cass_iterator_get_value(iterator.get())) == primary_type);
      BOOST_REQUIRE(test_utils::Value<T>::equal(result_value, values[i++]));
    }
    BOOST_REQUIRE(i == values.size());
  } else if(type == CASS_VALUE_TYPE_SET) {
    while(cass_iterator_next(iterator.get())) {
      T result_value;
      BOOST_REQUIRE(test_utils::Value<T>::get(cass_iterator_get_value(iterator.get()), &result_value) == CASS_OK);
      BOOST_REQUIRE(cass_value_type(cass_iterator_get_value(iterator.get())) == primary_type);
      BOOST_REQUIRE(std::find(values.begin(), values.end(), result_value) != values.end());
    }
  } else {
    BOOST_REQUIRE(false);
  }
}

void insert_collection_all_types(CassCluster* cluster, CassValueType type) {
  test_utils::CassFuturePtr session_future = test_utils::make_shared(cass_cluster_connect(cluster));
  test_utils::wait_and_check_error(session_future.get());
  test_utils::CassSessionPtr session = test_utils::make_shared(cass_future_get_session(session_future.get()));

  test_utils::execute_query(session.get(), str(boost::format(test_utils::CREATE_KEYSPACE_SIMPLE_FORMAT)
                                               % test_utils::SIMPLE_KEYSPACE % "1"));

  test_utils::execute_query(session.get(), str(boost::format("USE %s") % test_utils::SIMPLE_KEYSPACE));

  {
    std::vector<cass_int32_t> values;
    for(cass_int32_t i = 1; i <= 3; ++i) values.push_back(i);
    insert_collection_value<cass_int32_t>(session.get(), type, CASS_VALUE_TYPE_INT, values);
  }
  {
    std::vector<cass_int64_t> values;
    for(cass_int64_t i = 1LL; i <= 3LL; ++i) values.push_back(i);
    insert_collection_value<cass_int64_t>(session.get(), type, CASS_VALUE_TYPE_BIGINT, values);
  }
  {
    std::vector<cass_float_t> values;
    values.push_back(0.1f);
    values.push_back(0.2f);
    values.push_back(0.3f);
    insert_collection_value<cass_float_t>(session.get(), type, CASS_VALUE_TYPE_FLOAT, values);
  }
  {
    std::vector<cass_double_t> values;
    values.push_back(0.000000000001);
    values.push_back(0.000000000002);
    values.push_back(0.000000000003);
    insert_collection_value<cass_double_t>(session.get(), type, CASS_VALUE_TYPE_DOUBLE, values);
  }
  {
    std::vector<CassString> values;
    values.push_back(cass_string_init("abc"));
    values.push_back(cass_string_init("def"));
    values.push_back(cass_string_init("ghi"));
    insert_collection_value<CassString>(session.get(), type, CASS_VALUE_TYPE_VARCHAR,  values);
  }
  {
    std::vector<CassBytes> values;
    values.push_back(test_utils::bytes_from_string("123"));
    values.push_back(test_utils::bytes_from_string("456"));
    values.push_back(test_utils::bytes_from_string("789"));
    insert_collection_value<CassBytes>(session.get(), type, CASS_VALUE_TYPE_BLOB,  values);
  }
  {
    std::vector<CassInet> values;
    values.push_back(test_utils::inet_v4_from_int(16777343));
    values.push_back(test_utils::inet_v4_from_int(16777344));
    values.push_back(test_utils::inet_v4_from_int(16777345));
    insert_collection_value<CassInet>(session.get(), type, CASS_VALUE_TYPE_INET,  values);
  }
  {
    std::vector<test_utils::Uuid> values;
    for(int i = 0; i < 3; ++i) {
      values.push_back(test_utils::generate_time_uuid());
    }
    insert_collection_value<test_utils::Uuid>(session.get(), type, CASS_VALUE_TYPE_UUID,  values);
  }
  {
    const cass_uint8_t varint[] = { 57, 115, 235, 135, 229, 215, 8, 125, 13, 43, 1, 25, 32, 135, 129, 180,
                                    112, 176, 158, 120, 246, 235, 29, 145, 238, 50, 108, 239, 219, 100, 250,
                                    84, 6, 186, 148, 76, 230, 46, 181, 89, 239, 247 };
    std::vector<CassDecimal> values;
    for(int i = 0; i < 3; ++i) {
      CassDecimal value;
      value.scale = 100 + i;
      value.varint = cass_bytes_init(varint, sizeof(varint));
      values.push_back(value);
    }
    insert_collection_value<CassDecimal>(session.get(), type, CASS_VALUE_TYPE_DECIMAL,  values);
  }
}

template<class K, class V>
void insert_map_value(CassSession* session, CassValueType primary_type, CassValueType secondary_type, const std::map<K, V>& values) {
  std::string table_name = str(boost::format("%s_%s_%s") % test_utils::SIMPLE_TABLE
                                                         % test_utils::get_value_type(primary_type)
                                                         % test_utils::get_value_type(secondary_type));
  std::string type_name = str(boost::format("%s<%s, %s>") % test_utils::get_value_type(CASS_VALUE_TYPE_MAP)
                                                                % test_utils::get_value_type(primary_type)
                                                                % test_utils::get_value_type(secondary_type));

  test_utils::execute_query(session, str(boost::format("CREATE TABLE %s (tweet_id int PRIMARY KEY, test_val %s);")
                                         % table_name % type_name));

  test_utils::CassCollectionPtr input = test_utils::make_shared(cass_collection_new(CASS_COLLECTION_TYPE_MAP, values.size()));

  for(typename std::map<K, V>::const_iterator it = values.begin(),
      end = values.end(); it != end; ++it) {
    test_utils::Value<K>::append(input.get(), it->first);
    test_utils::Value<V>::append(input.get(), it->second);
  }

  std::string query = str(boost::format("INSERT INTO %s (tweet_id, test_val) VALUES(0, ?);") % table_name);

  test_utils::CassStatementPtr statement = test_utils::make_shared(cass_statement_new(cass_string_init(query.c_str()), 1));

  BOOST_REQUIRE(cass_statement_bind_collection(statement.get(), 0, input.get()) == CASS_OK);

  test_utils::CassFuturePtr result_future = test_utils::make_shared(cass_session_execute(session, statement.get()));
  test_utils::wait_and_check_error(result_future.get());

  test_utils::CassResultPtr result;
  test_utils::execute_query(session, str(boost::format("SELECT * FROM %s WHERE tweet_id = 0;") % table_name), &result);
  BOOST_REQUIRE(cass_result_row_count(result.get()) == 1);
  BOOST_REQUIRE(cass_result_column_count(result.get()) > 0);

  const CassRow* row = cass_result_first_row(result.get());

  const CassValue* output = cass_row_get_column(row, 1);
  BOOST_REQUIRE(cass_value_primary_sub_type(output) == primary_type);
  BOOST_REQUIRE(cass_value_secondary_sub_type(output) == secondary_type);

  test_utils::CassIteratorPtr iterator = test_utils::make_shared(cass_iterator_from_collection(output));

  size_t i = 0;
  while(cass_iterator_next(iterator.get())) {
    K result_key;
    V result_value;

    BOOST_REQUIRE(test_utils::Value<K>::get(cass_iterator_get_value(iterator.get()), &result_key) == CASS_OK);
    BOOST_REQUIRE(cass_value_type(cass_iterator_get_value(iterator.get())) == primary_type);

    BOOST_REQUIRE(cass_iterator_next(iterator.get()) == cass_true);
    BOOST_REQUIRE(test_utils::Value<V>::get(cass_iterator_get_value(iterator.get()), &result_value) == CASS_OK);
    BOOST_REQUIRE(cass_value_type(cass_iterator_get_value(iterator.get())) == secondary_type);

    typename std::map<K, V>::const_iterator it = values.find(result_key);
    BOOST_REQUIRE(it != values.end());
    BOOST_REQUIRE(test_utils::Value<V>::equal(result_value, it->second));
    i++;
  }
  BOOST_REQUIRE(i == values.size());
}

void insert_map_all_types(CassCluster* cluster) {
  test_utils::CassFuturePtr session_future = test_utils::make_shared(cass_cluster_connect(cluster));
  test_utils::wait_and_check_error(session_future.get());
  test_utils::CassSessionPtr session = test_utils::make_shared(cass_future_get_session(session_future.get()));

  test_utils::execute_query(session.get(), str(boost::format(test_utils::CREATE_KEYSPACE_SIMPLE_FORMAT)
                                               % test_utils::SIMPLE_KEYSPACE % "1"));

  test_utils::execute_query(session.get(), str(boost::format("USE %s") % test_utils::SIMPLE_KEYSPACE));

  {
    std::map<cass_int32_t, cass_int32_t> values;
    values[1] = 2;
    values[3] = 4;
    values[5] = 6;
    insert_map_value<cass_int32_t, cass_int32_t>(session.get(), CASS_VALUE_TYPE_INT, CASS_VALUE_TYPE_INT, values);
  }

  {
    std::map<cass_int64_t, cass_int64_t> values;
    values[1LL] = 2LL;
    values[3LL] = 4LL;
    values[5LL] = 6LL;
    insert_map_value<cass_int64_t, cass_int64_t>(session.get(), CASS_VALUE_TYPE_BIGINT, CASS_VALUE_TYPE_BIGINT, values);
  }

  {
    std::map<cass_float_t, cass_float_t> values;
    values[0.1f] = 0.2f;
    values[0.3f] = 0.4f;
    values[0.5f] = 0.6f;
    insert_map_value<cass_float_t, cass_float_t>(session.get(), CASS_VALUE_TYPE_FLOAT, CASS_VALUE_TYPE_FLOAT, values);
  }

  {
    std::map<cass_double_t, cass_double_t> values;
    values[0.000000000001] = 0.000000000002;
    values[0.000000000003] = 0.000000000004;
    values[0.000000000005] = 0.000000000006;
    insert_map_value<cass_double_t, cass_double_t>(session.get(), CASS_VALUE_TYPE_DOUBLE, CASS_VALUE_TYPE_DOUBLE, values);
  }

  {
    std::map<CassString, CassString> values;
    values[cass_string_init("abc")] = cass_string_init("123");
    values[cass_string_init("def")] = cass_string_init("456");
    values[cass_string_init("ghi")] = cass_string_init("789");
    insert_map_value<CassString, CassString>(session.get(), CASS_VALUE_TYPE_VARCHAR, CASS_VALUE_TYPE_VARCHAR, values);
  }

  {
    std::map<CassBytes, CassBytes> values;
    values[test_utils::bytes_from_string("abc")] = test_utils::bytes_from_string("123");
    values[test_utils::bytes_from_string("def")] = test_utils::bytes_from_string("456");
    values[test_utils::bytes_from_string("ghi")] = test_utils::bytes_from_string("789");
    insert_map_value<CassBytes, CassBytes>(session.get(), CASS_VALUE_TYPE_BLOB, CASS_VALUE_TYPE_BLOB, values);
  }

  {
    std::map<CassInet, CassInet> values;
    values[test_utils::inet_v4_from_int(16777343)] = test_utils::inet_v4_from_int(16777344);
    values[test_utils::inet_v4_from_int(16777345)] = test_utils::inet_v4_from_int(16777346);
    values[test_utils::inet_v4_from_int(16777347)] = test_utils::inet_v4_from_int(16777348);
    insert_map_value<CassInet, CassInet>(session.get(), CASS_VALUE_TYPE_INET, CASS_VALUE_TYPE_INET, values);
  }

  {
    std::map<test_utils::Uuid, test_utils::Uuid> values;
    values[test_utils::generate_time_uuid()] = test_utils::generate_random_uuid();
    values[test_utils::generate_time_uuid()] = test_utils::generate_random_uuid();
    values[test_utils::generate_time_uuid()] = test_utils::generate_random_uuid();
    insert_map_value<test_utils::Uuid, test_utils::Uuid>(session.get(), CASS_VALUE_TYPE_UUID, CASS_VALUE_TYPE_UUID, values);
  }

  {
    const cass_uint8_t varint1[] = { 57, 115, 235, 135, 229, 215, 8, 125, 13, 43, 1, 25, 32, 135, 129, 180 };
    const cass_uint8_t varint2[] = { 112, 176, 158, 120, 246, 235, 29, 145, 238, 50, 108, 239, 219, 100, 250 };
    const cass_uint8_t varint3[] = { 84, 6, 186, 148, 76, 230, 46, 181, 89, 239, 247 };

    std::map<CassDecimal, CassDecimal> values;
    values[test_utils::decimal_from_scale_and_bytes(0, cass_bytes_init(varint1, sizeof(varint1)))]
      = test_utils::decimal_from_scale_and_bytes(1, cass_bytes_init(varint1, sizeof(varint1)));
    values[test_utils::decimal_from_scale_and_bytes(2, cass_bytes_init(varint2, sizeof(varint2)))]
      = test_utils::decimal_from_scale_and_bytes(3, cass_bytes_init(varint2, sizeof(varint2)));
    values[test_utils::decimal_from_scale_and_bytes(4, cass_bytes_init(varint3, sizeof(varint3)))]
      = test_utils::decimal_from_scale_and_bytes(5, cass_bytes_init(varint3, sizeof(varint3)));
    insert_map_value<CassDecimal, CassDecimal>(session.get(), CASS_VALUE_TYPE_DECIMAL, CASS_VALUE_TYPE_DECIMAL, values);
  }

  {
    std::map<CassString, cass_int32_t> values;
    values[cass_string_init("a")] = 1;
    values[cass_string_init("b")] = 2;
    values[cass_string_init("c")] = 3;
    insert_map_value<CassString, cass_int32_t>(session.get(), CASS_VALUE_TYPE_VARCHAR, CASS_VALUE_TYPE_INT, values);
  }

  {
    std::map<test_utils::Uuid, CassString> values;
    values[test_utils::generate_time_uuid()] = cass_string_init("123"); 
    values[test_utils::generate_time_uuid()] = cass_string_init("456"); 
    values[test_utils::generate_time_uuid()] = cass_string_init("789"); 
    insert_map_value<test_utils::Uuid, CassString>(session.get(), CASS_VALUE_TYPE_UUID, CASS_VALUE_TYPE_VARCHAR, values);
  }
}

BOOST_AUTO_TEST_CASE(test_set)
{
  insert_collection_all_types(cluster, CASS_VALUE_TYPE_SET);
}

BOOST_AUTO_TEST_CASE(test_list)
{
  insert_collection_all_types(cluster, CASS_VALUE_TYPE_LIST);
}

BOOST_AUTO_TEST_CASE(test_map)
{
  insert_map_all_types(cluster);
}

BOOST_AUTO_TEST_SUITE_END()
