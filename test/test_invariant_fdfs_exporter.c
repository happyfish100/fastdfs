#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define TEST_PORT 9999
#define BUFFER_SIZE 1024

START_TEST(test_unauthenticated_requests_rejected)
{
    // Invariant: Protected endpoints reject unauthenticated requests
    const char *payloads[] = {
        "",  // Missing token
        "Bearer expired_token",  // Expired token
        "Bearer malformed_token",  // Malformed token
        "Bearer valid_token"  // Valid input (should still fail without proper validation)
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in server_addr;
        
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(TEST_PORT);
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
            char request[512];
            if (strlen(payloads[i]) > 0) {
                snprintf(request, sizeof(request),
                    "GET /metrics HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Authorization: %s\r\n"
                    "\r\n", payloads[i]);
            } else {
                snprintf(request, sizeof(request),
                    "GET /metrics HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "\r\n");
            }
            
            send(sockfd, request, strlen(request), 0);
            
            char response[BUFFER_SIZE];
            int bytes_received = recv(sockfd, response, sizeof(response) - 1, 0);
            response[bytes_received] = '\0';
            
            // Check for 401 Unauthorized or 403 Forbidden
            ck_assert_msg(strstr(response, "401") != NULL || strstr(response, "403") != NULL,
                         "Unauthenticated request with payload '%s' should be rejected", payloads[i]);
            
            close(sockfd);
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_unauthenticated_requests_rejected);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}