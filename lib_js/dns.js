let native = require('native');

const { customPromisifyArgs } = require('internal/util');
const errors = require('internal/errors');
const IANA_DNS_PORT = 53;
const IPv6RE = /^\[([^[\]]*)\]/;
const addrSplitRE = /(^.+?)(?::(\d+))?$/;
const {
    ERR_INVALID_IP_ADDRESS,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_OPT_VALUE
} = errors.codes;

class Resolver {
    cancel() {
        if (this._handle !== undefined)
            native.resolverCancel(this._handle);
    }

    getServers() {
        if (this._handle === undefined)
            this._handle = native.newResolver(this);
        return native.resolverGetServers(this._handle);
    }

    setServers(servers) {
        if (this._handle === undefined)
            this._handle = native.newResolver(this);

        const newSet = [];
        servers.forEach((serv) => {
            var ipVersion = native.isIP(serv);
            if (ipVersion !== 0)
                return newSet.push([ipVersion, serv, IANA_DNS_PORT]);

            const match = serv.match(IPv6RE);

            // Check for an IPv6 in brackets.
            if (match) {
                ipVersion = native.isIP(match[1]);

                if (ipVersion !== 0) {
                    const port =
                        parseInt(serv.replace(addrSplitRE, '$2')) ||
                        IANA_DNS_PORT;
                    return newSet.push([ipVersion, match[1], port]);
                }
            }

            // addr::port
            const addrSplitMatch = serv.match(addrSplitRE);

            if (addrSplitMatch) {
                const hostIP = addrSplitMatch[1];
                const port = addrSplitMatch[2] || IANA_DNS_PORT;

                ipVersion = native.isIP(hostIP);
                if (ipVersion !== 0) {
                    return newSet.push([ipVersion, hostIP, parseInt(port)]);
                }
            }

            throw new ERR_INVALID_IP_ADDRESS(serv);
        });
        native.resolverSetServers(this._handle, newSet);
    }

    reverse(ip, callback) {
        if (this._handle === undefined)
            this._handle = native.newResolver(this);

        native.resolverGetHostByAddr(this, ip, callback);
    }
}

function lookup(hostname, options, callback) {
    let family = 0;
    let hints = module.exports.ADDRCONFIG | module.exports.V4MAPPED;
    let all = false, verbatim = false;

    if (callback === undefined)
        callback = options;
    else if (typeof options !== 'object')
        family = options;
    else {
        if (options.hints !== undefined)
            hints = options.hints;
        if (options.family !== undefined)
            family = options.family;
        all = options.all;
        verbatim = options.verbatim;
    }

    native.lookup(hostname, family, hints, (err, addresses) => {
        if (err) {
            callback(err);
            return;
        }

        if (!verbatim)
            addresses.sort((a, b) => { return a.family - b.family; });
        if (all)
            callback(null, addresses);
        else
            callback(null, addresses[0].address, addresses[0].family);
    });
}

Object.defineProperty(lookup, customPromisifyArgs,
    { value: ['address', 'family'], enumerable: false });

function resolver(type) {
    function query(name, /* options, */ callback) {
        if (this._handle === undefined)
            this._handle = native.newResolver(this);

        var options;
        if (arguments.length > 2) {
            options = callback;
            callback = arguments[2];
        }

        let ttl = !!(options && options.ttl);
        native.resolverResolve(this, name, type, ttl, callback);
    }
    Object.defineProperty(query, 'name', { value: 'query' + type });
    return query;
}

var resolveMap = Object.create(null);
Resolver.prototype.resolveAny = resolveMap.ANY = resolver('ANY');
Resolver.prototype.resolve4 = resolveMap.A = resolver('A');
Resolver.prototype.resolve6 = resolveMap.AAAA = resolver('AAAA');
Resolver.prototype.resolveCname = resolveMap.CNAME = resolver('CNAME');
Resolver.prototype.resolveMx = resolveMap.MX = resolver('MX');
Resolver.prototype.resolveNs = resolveMap.NS = resolver('NS');
Resolver.prototype.resolveTxt = resolveMap.TXT = resolver('TXT');
Resolver.prototype.resolveSrv = resolveMap.SRV = resolver('SRV');
Resolver.prototype.resolvePtr = resolveMap.PTR = resolver('PTR');
Resolver.prototype.resolveNaptr = resolveMap.NAPTR = resolver('NAPTR');
Resolver.prototype.resolveSoa = resolveMap.SOA = resolver('SOA');

function resolve(hostname, rrtype, callback) {
    var resolver;
    if (typeof rrtype === 'string') {
        resolver = resolveMap[rrtype];
    } else if (typeof rrtype === 'function') {
        resolver = resolveMap.A;
        callback = rrtype;
    } else {
        throw new ERR_INVALID_ARG_TYPE('rrtype', 'string', rrtype);
    }

    if (typeof resolver === 'function') {
        return resolver.call(this, hostname, callback);
    } else {
        throw new ERR_INVALID_OPT_VALUE('rrtype', rrtype);
    }
}
Resolver.prototype.resolve = resolve;

let defaultResolver = new Resolver();
const resolverKeys = [
    'getServers',
    'resolve',
    'resolveAny',
    'resolve4',
    'resolve6',
    'resolveCname',
    'resolveMx',
    'resolveNs',
    'resolveTxt',
    'resolveSrv',
    'resolvePtr',
    'resolveNaptr',
    'resolveSoa',
    'reverse'
];

function getDefaultResolver() {
    return defaultResolver;
}

function setDefaultResolver(resolver) {
    defaultResolver = resolver;
}

function bindDefaultResolver(target, source) {
    resolverKeys.forEach((key) => {
        target[key] = source[key].bind(defaultResolver);
    });
}

function defaultResolverSetServers(servers) {
    const resolver = new Resolver();

    resolver.setServers(servers);
    setDefaultResolver(resolver);
    bindDefaultResolver(module.exports, Resolver.prototype);
}

module.exports = {
    lookup,
    lookupService: native.lookupService,

    Resolver,
    setServers: defaultResolverSetServers,

    ADDRCONFIG: 1,
    V4MAPPED: 2,

    // Error codes
    NODATA: 'ENODATA',
    FORMERR: 'EFORMERR',
    SERVFAIL: 'ESERVFAIL',
    NOTFOUND: 'ENOTFOUND',
    NOTIMP: 'ENOTIMP',
    REFUSED: 'EREFUSED',
    BADQUERY: 'EBADQUERY',
    BADNAME: 'EBADNAME',
    BADFAMILY: 'EBADFAMILY',
    BADRESP: 'EBADRESP',
    CONNREFUSED: 'ECONNREFUSED',
    TIMEOUT: 'ETIMEOUT',
    EOF: 'EOF',
    FILE: 'EFILE',
    NOMEM: 'ENOMEM',
    DESTRUCTION: 'EDESTRUCTION',
    BADSTR: 'EBADSTR',
    BADFLAGS: 'EBADFLAGS',
    NONAME: 'ENONAME',
    BADHINTS: 'EBADHINTS',
    NOTINITIALIZED: 'ENOTINITIALIZED',
    LOADIPHLPAPI: 'ELOADIPHLPAPI',
    ADDRGETNETWORKPARAMS: 'EADDRGETNETWORKPARAMS',
    CANCELLED: 'ECANCELLED'
};

bindDefaultResolver(module.exports, getDefaultResolver());
