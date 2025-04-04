#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http.h"
#include "consts.h"
#include "urlroutes.h"

// first route is always route to handler errneous request
// for Last route, RouteHandler.url = NULL is must indecating end of array
static RouteHandler *url_routes = NULL;


Response *err_get_handler(Request *req){
    char response[] = "<html><body><h1>Opps! Something went wrong.</h1></body></html>";
    char response_len[10];
    Response *resp = resp_create(req);
    Buffer *body_buf = buffer_new(128);

    sprintf(response_len, "%d", strlen(response));
    resp_set_status_code(resp, BAD_REQUEST_SC);
    resp_add_header(resp, "Content-Type", "text/html");
    resp_add_header(resp, "Content-Length", response_len);
    resp_add_header(resp, "Connection", "close");
    
    buffer_append(body_buf, response, strlen(response));
    resp_set_body(resp, body_buf);

    return resp;
}

static RouteHandler err_route_handler = {
    ERR_ROUTE,
    {err_get_handler, NULL, NULL, NULL}};

RetState register_route_handlers(RouteHandler *rhs, int nrh)
{
    RouteHandler *rh;
    RouteHandler last_route = {NULL, {NULL, NULL, NULL, NULL}};
    if (url_routes != NULL)
        return ERR_S;

    // 2 : 1 for err_route_handler, and 1 for last route
    rh = url_routes = (RouteHandler *)malloc(sizeof(RouteHandler) * (nrh+2));

    if (!url_routes)
        return ERR_S;

    *rh++ = err_route_handler;

    for (int i = 0; i < nrh; i++)
        *rh++ = rhs[i];

    *rh = last_route;
    return OK_S;
}

RequestHandler get_route_handler(char *url, Method method)
{
    RouteHandler *rh = url_routes;

    if (method == UNKNOWN_METHOD)
        return NULL;

    for (; rh->url != NULL; rh++)
    {
        if ((strcmp(rh->url, url) == 0) && rh->handlers[method])
            return rh->handlers[method];
    }
    return NULL;
}

void destroy_routes()
{
    if(url_routes)
        free(url_routes);
}
