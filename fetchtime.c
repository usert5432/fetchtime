#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "ntp.h"

struct timeval;

struct CMDArgs
{
    char *hostname;
    bool set_time;
    bool pprint_time;
    bool print_utime;
};

bool get_secure_random_uint32(u_int32_t *result)
{
    int fd = open("/dev/urandom", 0);
    if (fd == -1) {
        goto exit;
    }

    int ret = read(fd, result, sizeof(u_int32_t));
    close(fd);

    if (ret != sizeof(u_int32_t)) {
        goto exit;
    }

    return result;

exit:
    perror("Failed to read /dev/urandom");
    return false;
}

/* Transfers ownership of result on success. Returns NULL on failure. */
struct addrinfo* resolve_ntp_host(const char *hostname)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    int ret = getaddrinfo(hostname, "ntp", &hints, &result);
    if (ret != 0)
    {
        fprintf(stderr, "Failed to resolve hostname: %s\n", gai_strerror(ret));
        return NULL;
    }

    return result;
}

int connect_to_host(const struct addrinfo *addr_ntphost)
{
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock_fd == -1)
    {
        perror("Failed to open socket");
        return sock_fd;
    }

    int ret = connect(
        sock_fd, addr_ntphost->ai_addr, addr_ntphost->ai_addrlen
    );

    if (ret != 0)
    {
        perror("Failed to establish connection to ntp server");
        close(sock_fd);
        sock_fd = -1;
    }

    return sock_fd;
}

int connect_to_ntp_host(const char *hostname)
{
    int sock_fd = -1;

    struct addrinfo* hostinfo = resolve_ntp_host(hostname);
    if (hostinfo == NULL) {
        return sock_fd;
    }

    sock_fd = connect_to_host(hostinfo);

    freeaddrinfo(hostinfo);
    return sock_fd;
}

bool init_ntp_query(struct ntp_msg *query)
{
    memset(query, 0, sizeof(struct ntp_msg));

    query->status = MODE_CLIENT | (NTP_VERSION << 3);

    /* Do not leak host time */
    u_int32_t random_time;
    if (! get_secure_random_uint32(&random_time)) {
        return false;
    }
    query->xmttime.int_partl = random_time;

    /* Do not leak host time */
    if (! get_secure_random_uint32(&random_time)) {
        return false;
    }
    query->xmttime.fractionl = random_time;

    return true;
}

bool send_ntp_query(int sock_fd, const struct ntp_msg *query)
{
    int ret = send(sock_fd, query, sizeof(struct ntp_msg), 0);
    if (ret == -1)
    {
        perror("Failed to send ntp query");
        return false;
    }

    if (ret != sizeof(struct ntp_msg))
    {
        fprintf(stderr, "Failed to send complete ntp query\n");
        return false;
    }

    return true;
}

bool receive_ntp_response(int sock_fd, struct ntp_msg *response)
{
    int ret = recv(sock_fd, response, sizeof(struct ntp_msg), 0);
    if (ret == -1)
    {
        perror("Failed to receive ntp response");
        return false;
    }

    if (ret != sizeof(struct ntp_msg))
    {
        fprintf(stderr, "Incomplete ntp response received\n");
        return false;
    }

    return true;
}

bool verify_ntp_response(
    const struct ntp_msg *query, const struct ntp_msg *response
)
{
    u_int8_t status_mask = MODE_SERVER | (NTP_VERSION << 3);

    if ((response->status & status_mask) != status_mask)
    {
        fprintf(stderr, "ntp response has invalid flags\n");
        return false;
    }

    if (
           (response->orgtime.int_partl != query->xmttime.int_partl)
        || (response->orgtime.fractionl != query->xmttime.fractionl)
    )
    {
        fprintf(stderr, "ntp response does not match query\n");
        return false;
    }

    return true;
}

void extract_time_from_ntp_response(
    const struct ntp_msg *response, struct timeval *ntp_time
)
{
    ntp_time->tv_sec  = ntohl(response->xmttime.int_partl) - JAN_1970;
    ntp_time->tv_usec = (
        1.0e6 * ntohl(response->xmttime.fractionl) / UINT_MAX
    );
}

bool retrieve_time_from_ntp_server(
    int sock_fd, const struct ntp_msg *query, struct timeval *ntp_time
)
{
    struct ntp_msg response;

    if (! send_ntp_query(sock_fd, query)) {
        return false;
    }

    if (! receive_ntp_response(sock_fd, &response)) {
        return false;
    }

    if (! verify_ntp_response(query, &response)) {
        return false;
    }

    extract_time_from_ntp_response(&response, ntp_time);

    return true;
}

bool fetch_ntp_time(const char *hostname, struct timeval *ntp_time)
{
    int sock_fd = -1;
    struct ntp_msg query;

    if (! init_ntp_query(&query)) {
        return false;
    }

    sock_fd = connect_to_ntp_host(hostname);
    if (sock_fd == -1) {
        return false;
    }

    bool result = retrieve_time_from_ntp_server(sock_fd, &query, ntp_time);

    close(sock_fd);
    return result;
}

void set_system_time(const struct timeval *ntp_time)
{
    int ret = settimeofday(ntp_time, NULL);

    if (ret == -1) {
        perror("Failed to set system time");
    }
}

void print_time(const struct timeval *ntp_time, bool pprint, bool print_utime)
{
    time_t unixtime = ntp_time->tv_sec + ((double) ntp_time->tv_usec) / 1e6;

    if (pprint) {
        printf("%s", ctime(&unixtime));
    }

    if (print_utime) {
        printf("%lu\n", unixtime);
    }
}

void usage()
{
    puts(
"Usage: fetchtime [-h] [-p] [-s] [-u] HOST\n\
\n\
fetchtime is a small program to query time from the NTP server HOST.\n\
fetchtime can set the system clock from the returned NTP time,\n\
or simply print the returned time to stdout. \n\
\n\
  -h                   Show this help message.\n\
  -p                   Pretty print NTP time to stdout (default).\n\
  -s                   Set the system clock from the NTP time.\n\
  -u                   Print NTP time as unixtime to stdout.\n\
");

}

void die_with_usage(const char *fmt, ...)
{
    usage();

    va_list args;
    va_start(args, fmt);

    vfprintf(stderr, fmt, args);

    va_end(args);

    exit(EXIT_FAILURE);
}

struct CMDArgs parse_cmdargs(int argc, char **argv)
{
    struct CMDArgs result = {
        .pprint_time = false,
        .print_utime = false,
        .set_time    = false,
        .hostname    = NULL
    };

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            if (strlen(argv[i]) != 2) {
                die_with_usage("Unknown argument: %s\n", argv[i]);
            }

            if (argv[i][1] == 'h')
            {
                usage();
                exit(EXIT_SUCCESS);
            }

            if (argv[i][1] == 'p')
            {
                result.pprint_time = true;
                result.print_utime = false;
            }
            else if (argv[i][1] == 's') {
                result.set_time = true;
            }
            else if (argv[i][1] == 'u')
            {
                result.print_utime = true;
                result.pprint_time = false;
            }
            else {
                die_with_usage("Unknown argument: %s\n", argv[i]);
            }
        }
        else
        {
            if (result.hostname == NULL) {
                result.hostname = argv[i];
            }
            else {
                die_with_usage("Unknown command: %s\n", argv[i]);
            }
        }
    }

    if ((! result.set_time) && (! result.print_utime)) {
        result.pprint_time = true;
    }

    if (result.hostname == NULL) {
        die_with_usage("No hostname specified\n");
    }

    return result;
}

int main(int argc, char **argv)
{
    const struct CMDArgs cmdargs = parse_cmdargs(argc, argv);
    struct timeval ntp_time;

    if (! fetch_ntp_time(cmdargs.hostname, &ntp_time)) {
        exit(EXIT_FAILURE);
    }

    if (cmdargs.set_time) {
        set_system_time(&ntp_time);
    }

    print_time(&ntp_time, cmdargs.pprint_time, cmdargs.print_utime);
}

