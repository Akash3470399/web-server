#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "urlroutes.h"
#include "dtypes.h"
#include "consts.h"
#include "buffer.h"
#include "util.h"

#define BODY_COPY_BUFLEN 1024

typedef struct Header Header;

struct Header
{
    char *key, *value;
    Header *next;
};

struct Request
{
    Method method;
    HttpVersion http_version;
    uchar uri[URILEN];
    Header *headers;
    Buffer *body;
};

struct Response
{
    HttpVersion http_version;
    StatusCode status_code;
    Header *headers;
    Buffer *body;
};

// globals
char *method_str[] = {
    [GET] = "GET",
    [HEAD] = "HEAD",
    [POST] = "POST",
    [PUT] = "PUT",
    [DELETE] = "DELETE",
    NULL};

char *http_version_str[] = {
    [HTTP_1_1] = "HTTP/1.1",
    [HTTP_2_0] = "HTTP/2",
    NULL};

char *status_code_str[] = {
    [OK_SC] = "200 OK",
    [BAD_REQUEST_SC] = "400 Bad Request",
    [NOT_FOUND_SC] = "404 Not Found",
    [INTERNAL_SERVER_ERROR_SC] = "500 Internal Server Error",
};

// ******* REQUEST/RESPONSE HELPER FUNCTIONS **************
Method str_to_Method(uchar *method)
{
    for (int i = 0; method_str[i] != NULL; i++)
        if (!(strcmp(method, method_str[i])))
            return i;
    return UNKNOWN_METHOD;
}

static RetState check_field_validity(uchar *name, uchar *value)
{
    if ((name == NULL) || (value == NULL))
        return ERR_S;

    for (uint i = 0; i < strlen(name); i++)
        if ((name[i] == SPACE) || (name[i] == TAB))
            return ERR_S;

    uchar rm_set[] = {SPACE, TAB, '\0'};
    strip_bytes(value, strlen(value), rm_set, strlen(rm_set));

    for (uint i = 0; i < strlen(value); i++)
        if ((value[i] == SPACE) || (value[i] == TAB))
            return ERR_S;

    return OK_S;
}

static Header *parse_headers(Buffer *buf)
{
    uchar *field, done = 0, *value, *key;
    int field_len;
    Header *h = NULL;

    while (1)
    {
        if ((field_len = buffer_read_till_delim(buf, &field, NEW_LINE)) < 0)
            break;

        // \r\n (end of headers section)
        if ((field_len <= 1) && (*field == CARRIAGE_RET))
            break;

        // colon (:) separates key and value
        if ((value = memchr(field, ':', field_len)) != NULL)
        {
            key = field;
            *value = '\0';
            value += 1;
            if (check_field_validity(key, value) > 0)
            {
                Header *new_h = (Header *)malloc(sizeof(Header));
                new_h->key = strdup(key);
                new_h->value = strdup(value);
                new_h->next = h;
                h = new_h;
            }
        }
        free(field);
    }

    return h;
}

static RetState parse_request_line(Buffer *buf, Request *req)
{
    uchar *token = NULL;
    int token_len;
    uchar rm_set[] = {' '};

    // HTTP METHOD
    if ((token_len = buffer_read_till_delim(buf, &token, SPACE)) == -1)
        return ERR_S;
    if ((req->method = str_to_Method(token)) == UNKNOWN_METHOD)
    {
        free(token);
        return ERR_S;
    }
    free(token);

    // URI
    if ((token_len = buffer_read_till_delim(buf, &token, SPACE)) == -1)
        return ERR_S;
    if (token_len < URILEN)
        memmove(req->uri, token, token_len);
    else
    {
        free(token);
        return ERR_S;
    }
    free(token);

    // HTTP VERSION
    if ((token_len = buffer_read_till_delim(buf, &token, NEW_LINE)) == -1)
        return ERR_S;
    if (!strcmp(token, "HTTP/1.1\r"))
        req->http_version = HTTP_1_1;
    else if (!strcmp(token, "HTTP/2\r"))
        req->http_version = HTTP_2_0;
    else
    {
        free(token);
        return ERR_S;
    }
    free(token);

    return OK_S;
}

static void free_headers(Header *h)
{
    Header *next;
    while (h)
    {
        next = h->next;
        free(h->key);
        free(h->value);
        free(h);
        h = next;
    }
}

RetState get_url_path(char *path, char *url)
{
    RetState ret = OK_S;
    char *p1, *query;
    memset(path, 0, URILEN);

    p1 = strchr(url, '/');
    query = strchr(p1, '?');

    if (p1 && query)
        strncpy(path, p1, query - p1);
    else if (p1 && !query)
        strcpy(path, url);
    else
        ret = ERR_S;

    return ret;
}

RetState serialize_status_line(Response *resp, Buffer *resp_buf)
{
    // status-line = HTTP-version SP status-code SP [ reason-phrase ] CRLF
    // reason-phrase is textual phrase describing the status code.

    char status_line[STATUS_LINE_LEN] = {0};

    if (
        (resp->http_version < HTTP_1_1) || (resp->http_version > HTTP_2_0) ||
        (resp->status_code < OK_SC) || (resp->status_code >= UNKNOWN_STATUS_CODE)
        )
        return ERR_S;

    strcat(status_line, http_version_str[resp->http_version]);
    strcat(status_line, " ");
    strcat(status_line, status_code_str[resp->status_code]);
    strcat(status_line, CRLF);

    if (buffer_append(resp_buf, status_line, strlen(status_line)) == -1)
        return ERR_S;
    return OK_S;
}

RetState serialize_headers(Response *resp, Buffer *resp_buf)
{
    // field-line   = field-name ":" OWS field-value OWS
    Header *h = resp->headers;
    int r = 0;

    while (h && (r != -1))
    {
        r |= buffer_append(resp_buf, h->key, strlen(h->key));
        r |= buffer_append(resp_buf, ":", 1);
        r |= buffer_append(resp_buf, h->value, strlen(h->value));
        r |= buffer_append(resp_buf, CRLF, strlen(CRLF));
        h = h->next;
    }

    return ((r != -1) ? OK_S : ERR_S);
}
// **************** REQUEST **********************

// function defined in urlroutes.c used in handle_request
RequestHandler get_route_handler(char *url, Method method);

// http.h functions
Request *parse_request(Buffer *req_buf)
{
    Request *req;
    uint body_offset, body_len;
    int failed = 0;

    if ((req = (Request *)malloc(sizeof(Request))) == NULL)
        return NULL;

    // REQUEST LINE
    if (parse_request_line(req_buf, req) == ERR_S)
        failed = 1;

    // REQUEST HEADERS
    if ((!failed) && ((req->headers = parse_headers(req_buf)) == NULL))
        failed = 1;

    if (failed)
    {
        free(req);
        return NULL;
    }

    // REQUEST BODY
    body_offset = buffer_seek(req_buf, 0, Current_P);
    body_len = (buffer_datasize(req_buf) - body_offset);

    if (body_len > 0)
    {
        req->body = buffer_new(body_len);
        failed = buffer_copy(req->body, req_buf, body_offset, body_len);
    }
    else
        req->body = NULL;

    if ((body_len > 0) && (failed == -1))
    {
        free_headers(req->headers);
        free(req);
        return NULL;
    }

    return req;
}

Response *handle_request(Request *req)
{
    char url[URILEN] = {0};
    RequestHandler handler;

    get_url_path(url, req->uri);
    if ((handler = get_route_handler(url, req->method)) != NULL)
        return handler(req);

    handler = get_route_handler(ERR_ROUTE, GET);
    return handler(req);
}

Buffer *serialize_response(Response *resp)
{
    Buffer *resp_buf = buffer_new(32);
    RetState ret = 0;
    int r = 0;

    ret |= serialize_status_line(resp, resp_buf);
    ret |= serialize_headers(resp, resp_buf);

    r = buffer_append(resp_buf, CRLF, strlen(CRLF));
    if (resp->body && (r != -1))
        r |= buffer_copy(resp_buf, resp->body, 0, buffer_datasize(resp->body));


    if ((ret != OK_S) || (r == -1))
    {
        buffer_destroy(resp_buf);
        return NULL;
    }

    return resp_buf;
}

// Response helper functions

Response *resp_create(Request *req)
{
    Response *resp = NULL;
    if (req)
    {
        if ((resp = (Response *)malloc(sizeof(Response))) != NULL)
        {
            resp->headers = NULL;
            resp->http_version = req->http_version;
            resp->status_code = INTERNAL_SERVER_ERROR_SC;
            resp->body = NULL;
        }
    }
    return resp;
}

RetState resp_add_header(Response *resp, char *key, char *value)
{
    RetState state = ERR_S;
    Header *h = NULL;

    if (resp && key && value)
    {
        if ((h = (Header *)malloc(sizeof(Header))) != NULL)
        {
            h->key = strdup(key);
            h->value = strdup(value);
            h->next = resp->headers;
            resp->headers = h;

            state = OK_S;
        }
    }

    return state;
}

RetState resp_set_status_code(Response *resp, StatusCode scode)
{
    if (!resp)
        return ERR_S;
    resp->status_code = scode;
    return OK_S;
}

RetState resp_set_body(Response *resp, Buffer *body)
{
    RetState state = ERR_S;

    if (resp && body)
    {
        uint body_len = buffer_datasize(body);
        if (!resp->body)
            resp->body = buffer_new(body_len);

        if (buffer_copy(resp->body, body, 0, body_len) != -1)
            state = OK_S;
    }

    return state;
}

void free_request(Request *req)
{
    if (req)
    {
        free_headers(req->headers);
        buffer_destroy(req->body);
        free(req);
    }
}

void free_response(Response *resp)
{
    if (resp)
    {
        free_headers(resp->headers);
        buffer_destroy(resp->body);
        free(resp);
    }
}