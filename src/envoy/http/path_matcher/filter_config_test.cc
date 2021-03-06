// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/envoy/http/path_matcher/filter_config.h"
#include "test/mocks/server/mocks.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace PathMatcher {
namespace {

using ::google::api_proxy::path_matcher::VariableBinding;
using ::google::protobuf::TextFormat;
typedef std::vector<VariableBinding> VariableBindings;
typedef std::vector<std::string> FieldPath;

TEST(FilterConfigTest, EmptyConfig) {
  ::google::api::envoy::http::path_matcher::FilterConfig config_pb;
  ::testing::NiceMock<Server::Configuration::MockFactoryContext> mock_factory;
  FilterConfig cfg(config_pb, "", mock_factory);

  EXPECT_TRUE(cfg.findOperation("GET", "/foo") == nullptr);
  EXPECT_TRUE(cfg.getSnakeToJsonMap().empty());
}

TEST(FilterConfigTest, BasicConfig) {
  const char kFilterConfigBasic[] = R"(
rules {
  operation: "1.cloudesf_testing_cloud_goog.Bar"
  pattern {
    http_method: "GET"
    uri_template: "/bar"
  }
}
rules {
  operation: "1.cloudesf_testing_cloud_goog.Foo"
  extract_path_parameters: true
  pattern {
    http_method: "GET"
    uri_template: "/foo/{id}"
  }
})";

  ::google::api::envoy::http::path_matcher::FilterConfig config_pb;
  ASSERT_TRUE(TextFormat::ParseFromString(kFilterConfigBasic, &config_pb));
  ::testing::NiceMock<Server::Configuration::MockFactoryContext> mock_factory;
  FilterConfig cfg(config_pb, "", mock_factory);

  EXPECT_EQ("1.cloudesf_testing_cloud_goog.Bar",
            *cfg.findOperation("GET", "/bar"));
  EXPECT_EQ("1.cloudesf_testing_cloud_goog.Foo",
            *cfg.findOperation("GET", "/foo/xyz"));

  EXPECT_EQ(nullptr, cfg.findOperation("POST", "/bar"));
  EXPECT_EQ(nullptr, cfg.findOperation("POST", "/foo/xyz"));

  EXPECT_FALSE(
      cfg.needParameterExtraction("1.cloudesf_testing_cloud_goog.Bar"));
  EXPECT_TRUE(cfg.needParameterExtraction("1.cloudesf_testing_cloud_goog.Foo"));

  EXPECT_TRUE(cfg.getSnakeToJsonMap().empty());
}

TEST(FilterConfigTest, VariableBinding) {
  const char kFilterConfigBasic[] = R"(
rules {
  operation: "1.cloudesf_testing_cloud_goog.Foo"
  extract_path_parameters: true
  pattern {
    http_method: "GET"
    uri_template: "/foo/{id}"
  }
})";

  ::google::api::envoy::http::path_matcher::FilterConfig config_pb;
  ASSERT_TRUE(TextFormat::ParseFromString(kFilterConfigBasic, &config_pb));
  ::testing::NiceMock<Server::Configuration::MockFactoryContext> mock_factory;
  FilterConfig cfg(config_pb, "", mock_factory);

  VariableBindings bindings;
  EXPECT_EQ("1.cloudesf_testing_cloud_goog.Foo",
            *cfg.findOperation("GET", "/foo/xyz", &bindings));
  EXPECT_EQ(VariableBindings({
                VariableBinding{FieldPath{"id"}, "xyz"},
            }),
            bindings);

  // With query parameters
  EXPECT_EQ("1.cloudesf_testing_cloud_goog.Foo",
            *cfg.findOperation("GET", "/foo/xyz?zone=east", &bindings));
  EXPECT_EQ(VariableBindings({
                VariableBinding{FieldPath{"id"}, "xyz"},
            }),
            bindings);
}

TEST(FilterConfigTest, SegmentNames) {
  const char kFilterConfig[] = R"(
segment_names {
  json_name: "fooBar"
  snake_name: "foo_bar"
}
segment_names {
  json_name: "xYZ"
  snake_name: "x_y_z"
})";

  ::google::api::envoy::http::path_matcher::FilterConfig config_pb;
  ASSERT_TRUE(TextFormat::ParseFromString(kFilterConfig, &config_pb));
  ::testing::NiceMock<Server::Configuration::MockFactoryContext> mock_factory;
  FilterConfig cfg(config_pb, "", mock_factory);

  absl::flat_hash_map<std::string, std::string> expected = {
      {"foo_bar", "fooBar"}, {"x_y_z", "xYZ"}};
  EXPECT_EQ(cfg.getSnakeToJsonMap(), expected);
}

TEST(FilterConfigTest, DuplicatedPatterns) {
  const char kFilterConfig[] = R"(
rules {
  operation: "1.cloudesf_testing_cloud_goog.Bar"
  pattern {
    http_method: "GET"
    uri_template: "/bar/{id}"
  }
}
rules {
  operation: "1.cloudesf_testing_cloud_goog.Bar1"
  pattern {
    http_method: "GET"
    uri_template: "/bar/{x}"
  }
})";

  ::google::api::envoy::http::path_matcher::FilterConfig config_pb;
  ASSERT_TRUE(TextFormat::ParseFromString(kFilterConfig, &config_pb));
  ::testing::NiceMock<Server::Configuration::MockFactoryContext> mock_factory;

  EXPECT_THROW_WITH_REGEX(FilterConfig cfg(config_pb, "", mock_factory),
                          ProtoValidationException, "Duplicated pattern");
}

}  // namespace
}  // namespace PathMatcher
}  // namespace HttpFilters
}  // namespace Extensions
}  // namespace Envoy
