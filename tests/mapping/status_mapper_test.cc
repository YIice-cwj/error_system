#include "error_system/mapping/status_mapper.h"

#include <gtest/gtest.h>

#include "error_system/core/error_code.h"
#include "error_system/domain/system_domain.h"
#include "error_system/mapping/grpc_status.h"
#include "error_system/mapping/http_status.h"

namespace error_system::core {
    namespace {
        // 辅助：构造错误码（sign=0 表示错误）
        constexpr error_code_t make_err(error_level_t level,
                                        domain::system_domain_t system,
                                        bool retryable = false,
                                        bool transient = false) noexcept {
            error_code_t code(level, system, 1, 1, 1);
            code.set_retryable(retryable);
            code.set_transient(transient);
            return code;
        }
    }  // namespace
}  // namespace error_system::core

namespace error_system::mapping {
    using error_system::core::error_code_t;
    using error_system::core::error_level_t;
    using error_system::core::make_err;


    TEST(status_mapper_test, success_code_maps_to_http_ok) {
        error_code_t success = error_code_t::make_success();
        EXPECT_EQ(status_mapper_t::to_http_status(success), http_status_t::ok);
    }

    TEST(status_mapper_test, info_level_maps_to_http_ok) {
        EXPECT_EQ(status_mapper_t::to_http_status(
            make_err(error_level_t::info, domain::system_domain_t::application)),
            http_status_t::ok);
    }

    TEST(status_mapper_test, warn_level_maps_to_http_ok) {
        EXPECT_EQ(status_mapper_t::to_http_status(
            make_err(error_level_t::warn, domain::system_domain_t::system)),
            http_status_t::ok);
    }

    TEST(status_mapper_test, error_application_maps_to_http_bad_request) {
        EXPECT_EQ(status_mapper_t::to_http_status(
            make_err(error_level_t::error, domain::system_domain_t::application)),
            http_status_t::bad_request);
    }

    TEST(status_mapper_test, error_third_party_maps_to_http_bad_gateway) {
        EXPECT_EQ(status_mapper_t::to_http_status(
            make_err(error_level_t::error, domain::system_domain_t::third_party)),
            http_status_t::bad_gateway);
    }

    TEST(status_mapper_test, error_database_maps_to_http_service_unavailable) {
        EXPECT_EQ(status_mapper_t::to_http_status(
            make_err(error_level_t::error, domain::system_domain_t::database)),
            http_status_t::service_unavailable);
    }

    TEST(status_mapper_test, error_middleware_maps_to_http_service_unavailable) {
        EXPECT_EQ(status_mapper_t::to_http_status(
            make_err(error_level_t::error, domain::system_domain_t::middleware)),
            http_status_t::service_unavailable);
    }

    TEST(status_mapper_test, error_system_maps_to_http_internal_server_error) {
        EXPECT_EQ(status_mapper_t::to_http_status(
            make_err(error_level_t::error, domain::system_domain_t::system)),
            http_status_t::internal_server_error);
    }

    TEST(status_mapper_test, fatal_level_maps_to_http_internal_server_error) {
        EXPECT_EQ(status_mapper_t::to_http_status(
            make_err(error_level_t::fatal, domain::system_domain_t::database)),
            http_status_t::internal_server_error);
    }

    TEST(status_mapper_test, retryable_overrides_domain_to_http_service_unavailable) {
        // 即使是 application 域，retryable 应映射为 503
        EXPECT_EQ(status_mapper_t::to_http_status(
            make_err(error_level_t::error, domain::system_domain_t::application, true, false)),
            http_status_t::service_unavailable);
    }

    TEST(status_mapper_test, transient_overrides_domain_to_http_service_unavailable) {
        EXPECT_EQ(status_mapper_t::to_http_status(
            make_err(error_level_t::error, domain::system_domain_t::application, false, true)),
            http_status_t::service_unavailable);
    }


    TEST(status_mapper_test, success_code_maps_to_grpc_ok) {
        EXPECT_EQ(status_mapper_t::to_grpc_status(error_code_t::make_success()),
                  grpc_status_t::ok);
    }

    TEST(status_mapper_test, info_level_maps_to_grpc_ok) {
        EXPECT_EQ(status_mapper_t::to_grpc_status(
            make_err(error_level_t::info, domain::system_domain_t::application)),
            grpc_status_t::ok);
    }

    TEST(status_mapper_test, error_application_maps_to_grpc_invalid_argument) {
        EXPECT_EQ(status_mapper_t::to_grpc_status(
            make_err(error_level_t::error, domain::system_domain_t::application)),
            grpc_status_t::invalid_argument);
    }

    TEST(status_mapper_test, error_database_maps_to_grpc_data_loss) {
        EXPECT_EQ(status_mapper_t::to_grpc_status(
            make_err(error_level_t::error, domain::system_domain_t::database)),
            grpc_status_t::data_loss);
    }

    TEST(status_mapper_test, error_middleware_maps_to_grpc_unavailable) {
        EXPECT_EQ(status_mapper_t::to_grpc_status(
            make_err(error_level_t::error, domain::system_domain_t::middleware)),
            grpc_status_t::unavailable);
    }

    TEST(status_mapper_test, error_third_party_maps_to_grpc_unavailable) {
        EXPECT_EQ(status_mapper_t::to_grpc_status(
            make_err(error_level_t::error, domain::system_domain_t::third_party)),
            grpc_status_t::unavailable);
    }

    TEST(status_mapper_test, error_system_maps_to_grpc_internal) {
        EXPECT_EQ(status_mapper_t::to_grpc_status(
            make_err(error_level_t::error, domain::system_domain_t::system)),
            grpc_status_t::internal);
    }

    TEST(status_mapper_test, fatal_maps_to_grpc_internal) {
        EXPECT_EQ(status_mapper_t::to_grpc_status(
            make_err(error_level_t::fatal, domain::system_domain_t::database)),
            grpc_status_t::internal);
    }

    TEST(status_mapper_test, retryable_maps_to_grpc_unavailable) {
        EXPECT_EQ(status_mapper_t::to_grpc_status(
            make_err(error_level_t::error, domain::system_domain_t::application, true, false)),
            grpc_status_t::unavailable);
    }

    TEST(status_mapper_test, transient_maps_to_grpc_unavailable) {
        EXPECT_EQ(status_mapper_t::to_grpc_status(
            make_err(error_level_t::fatal, domain::system_domain_t::system, false, true)),
            grpc_status_t::unavailable);
    }


    TEST(status_mapper_test, constexpr_evaluation_works) {
        constexpr error_code_t code(error_level_t::error, domain::system_domain_t::database, 1, 1, 1);
        constexpr http_status_t http = status_mapper_t::to_http_status(code);
        constexpr grpc_status_t grpc = status_mapper_t::to_grpc_status(code);
        static_assert(http == http_status_t::service_unavailable, "database error should map to HTTP 503");
        static_assert(grpc == grpc_status_t::data_loss, "database error should map to gRPC DATA_LOSS");
        SUCCEED();
    }

}  // namespace error_system::mapping
