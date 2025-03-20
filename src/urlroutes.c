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
    Response *r = resp_create(req);
    resp_set_status_code(r, BAD_REQUEST_SC);
    return r;
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
