#ifndef URLROUTES_H
#define URLrOUTES_h

#include "dtypes.h"
#include "http.h"

typedef struct RouteHandler RouteHandler;

struct RouteHandler
{
    char *url;
    // array of request handler function pointers stored wrt enum Method
    // Here UNKNOWN_METHOD will be last method and it gives count of method supported
    Response *(*handlers[UNKNOWN_METHOD])(Request *);
};

RetState register_route_handlers(RouteHandler *rhs, int nrh);
void destroy_routes();

#endif