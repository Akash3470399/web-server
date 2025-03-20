#ifndef HTTP_H
#define HTTP_H

#include "buffer.h"

typedef struct Request Request;
typedef struct Response Response;
typedef Response *(*RequestHandler)(Request *);


// UNKNOWN_METHOD must be last, and enum value should be start with 0 till all method count
// not enum value modification allowd on enum Method eg. GET = 2 not allowed
typedef enum
{
    GET,
    HEAD,
    POST,
    PUT,
    DELETE,
    UNKNOWN_METHOD
} Method;

typedef enum
{
    OK_SC,
    BAD_REQUEST_SC,
    NOT_FOUND_SC,
    INTERNAL_SERVER_ERROR_SC,
    UNKNOWN_STATUS_CODE
} StatusCode;

typedef enum
{
    HTTP_1_1,
    HTTP_2_0
} HttpVersion;

Request *parse_request(Buffer *req_buf);

Response *handle_request(Request *req);

Buffer *serialize_response(Response *resp);


// Response helper functions
Response *resp_create(Request *req);
RetState resp_add_header(Response *resp, char *key, char *value);
RetState resp_set_status_code(Response *resp, StatusCode scode);
RetState resp_set_body(Response *resp, Buffer *body);

/**
 * @brief Frees the memory associated with a Request structure.
 * @param req A pointer to the `Request` structure to be freed.
 */
void free_request(Request *req);

/**
 * @brief Frees the memory associated with a Response structure.
 * @param resp A pointer to the `Response` structure to be freed.
 */
void free_response(Response *resp);

#endif