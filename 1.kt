var $jscomp = $jscomp ||
{};$jscomp.scope={};$jscomp.ASSUME_ES5=!1;$jscomp.ASSUME_NO_NATIVE_MAP=!1;$jscomp.ASSUME_NO_NATIVE_SET=!1;$jscomp.SIMPLE_FROUND_POLYFILL=!1;$jscomp.defineProperty=$jscomp.ASSUME_ES5||"function"==typeof Object.defineProperties?Object.defineProperty:function(b,a,f){
    b != Array.prototype && b != Object.prototype && (b[a] = f.value)
};$jscomp.getGlobal=function(b){ return "undefined" != typeof window && window === b ? b :"undefined" != typeof global && null != global?global:b };$jscomp.global=$jscomp.getGlobal(this);
$jscomp.polyfill=function(b,a,f,c){
    if (a) {
        f = $jscomp.global;b = b.split(".");for (c= 0;c < b.length - 1;c++){
            var e = b[c];e in f || (f[e] = {});f = f[e]
        } b = b [b.length - 1];c = f[b];a = a(c);a != c && null != a && $jscomp.defineProperty(
            f,
            b,
            { configurable:!0, writable:!0, value:a })
    }
};
$jscomp.polyfill("String.fromCodePoint",function(b){
    return b?b:function(b){
        for (var a= "", c = 0;c < arguments.length;c++){
        var e =
            Number(arguments[c]);if (0 > e || 1114111 < e || e !== Math.floor(e)) throw new RangeError ("invalid_code_point " + e);65535 >= e?a += String.fromCharCode(e):(e -= 65536, a += String.fromCharCode(e>>>10&1023|55296), a += String.fromCharCode(e&1023|56320))
    }return a
    }
},"es6","es3");$jscomp.arrayIteratorImpl=function(b){
    var a = 0;return function() { return a < b.length?{ done:!1, value:b[a++] }:{ done:!0 } }
};
$jscomp.arrayIterator=function(b){ return { next:$jscomp.arrayIteratorImpl(b) } };$jscomp.makeIterator=function(b){
    var a =
        "undefined" != typeof Symbol && Symbol . iterator && b [Symbol.iterator];return a?a.call(b):$jscomp.arrayIterator(b)
};$jscomp.FORCE_POLYFILL_PROMISE=!1;
$jscomp.polyfill("Promise",function(b){
    function a (){
        this.batch_ = null
    } function f(b) { return b instanceof e?b:new e(function(a, c){ a(b) }) }if (b && !$jscomp.FORCE_POLYFILL_PROMISE) return b;a.prototype.asyncExecute =
        function(b) {
            if (null == this.batch_) {
                this.batch_ = [];
                var a = this;this.asyncExecuteFunction(function() { a.executeBatch_() })
            }this.batch_.push(b)
        };
    var c =$jscomp.global.setTimeout;a.prototype.asyncExecuteFunction =
        function(b) { c(b, 0) };a.prototype.executeBatch_ = function() {
        for (;this.batch_ && this.batch_.length;){
        var b =
            this.batch_;this.batch_ = [];for (var a= 0;a < b.length;++a){
        var c = b[a];b[a] = null;try {
        c()
    } catch (k) {
        this.asyncThrow_(k)
    }
    }
    }this.batch_ = null
    };a.prototype.asyncThrow_ = function(b) { this.asyncExecuteFunction(function() { throw b; }) };
    var e = function(b) {
        this.state_ = 0;this.result_ = void 0;this.onSettledCallbacks_ = [];
        var a = this.createResolveAndReject_();try {
        b(a.resolve, a.reject)
    } catch (l) {
        a.reject(l)
    }
    };e.prototype.createResolveAndReject_ = function() {
        function b (b){ return function(d) { c || (c = !0, b.call(a, d)) } }
        var a = this, c = !1;
        return { resolve:b(this.resolveTo_), reject:b(this.reject_) }
    };e.prototype.resolveTo_ = function(b) {
        if (b === this) this.reject_(new TypeError ("A Promise cannot resolve to itself")); else if (b instanceof e) this.settleSameAsPromise_(
            b
        ); else {
            a:switch(typeof b){ case "object":var a = null != b;break a;case "function":a = !0;break a;default:a = !1 } a ?this.resolveToNonPromiseObj_(
                b
            ):this.fulfill_(b)
        }
    };e.prototype.resolveToNonPromiseObj_ = function(b) {
        var a = void 0;try {
        a = b.then
    } catch (l) {
        this.reject_(l);return
    }"function" == typeof a?
        this.settleSameAsThenable_(a, b):this.fulfill_(b)
    };e.prototype.reject_ = function(b) { this.settle_(2, b) };e.prototype.fulfill_ =
        function(b) { this.settle_(1, b) };e.prototype.settle_ = function(
        b,
        a
    ) {
        if (0 != this.state_) throw Error("Cannot settle(" + b + ", " + a + "): Promise already settled in state" + this.state_);this.state_ =
        b;this.result_ = a;this.executeOnSettledCallbacks_()
    };e.prototype.executeOnSettledCallbacks_ = function() {
        if (null != this.onSettledCallbacks_) {
            for (var b= 0;b < this.onSettledCallbacks_.length;++b)d.asyncExecute(this.onSettledCallbacks_[b]);
            this.onSettledCallbacks_ = null
        }
    };
    var d = new a;e.prototype.settleSameAsPromise_ = function(b) {
        var a = this.createResolveAndReject_();b.callWhenSettled_(
        a.resolve,
        a.reject
    )
    };e.prototype.settleSameAsThenable_ = function(b, a) {
        var c = this.createResolveAndReject_();try {
        b.call(a, c.resolve, c.reject)
    } catch (k) {
        c.reject(k)
    }
    };e.prototype.then = function(b, a) {
        function c (b, a){
        return "function" == typeof b ? function (a){
            try {
                d(b(a))
            } catch (n) {
                f(n)
            }
        }:a
    }
        var d, f, g = new e(function(b, a){ d = b;f = a });this.callWhenSettled_(
        c(b, d),
        c(a, f)
    );return g
    };
    e.prototype.catch = function(b) { return this.then(void 0, b) };e.prototype.callWhenSettled_ =
        function(
            b,
            a
        ) {
            function c (){ switch(e.state_) { case 1:b(e.result_);break;case 2:a(e.result_);break;default:throw Error("Unexpected state: "+e.state_); } }
            var e =
                this;null == this.onSettledCallbacks_?d.asyncExecute(c):this.onSettledCallbacks_.push(c)
        };e.resolve = f;e.reject = function(b) { return new e (function(a, c) { c(b) }) };e.race =
        function(b) {
            return new e (function(a, c) {
                for (var d=$jscomp.makeIterator(b), e = d.next();!e.done;e =
                d.next())f(e.value).callWhenSettled_(a,
                c)
            })
        };e.all = function(b) {
        var a =$jscomp.makeIterator(b), c = a.next();return c.done?f([]):new e(function(b, d){
        function e (a){
            return function(c) { g[a] = c;h--;0 == h && b(g) }
        }
        var g = [], h = 0;do g.push(
        void
        0
    ), h++, f(c.value).callWhenSettled_(e(g.length-1), d), c = a.next();while (!c.done)
    })
    };return e
},"es6","es3");
(function(b){
    function a (){ for (var b= 0;b < x.length;b++)x[b][0](x[b][1]);x = [];z = !1 } function f(
        b,
        c
    ) { x.push([b, c]);z || (z = !0, A(a, 0)) } function c(b, a) {
        function c (b){
            g(
                a,
                b
            )
        } function d(b) { l(a, b) }try {
        b(c, d)
    } catch (y) {
        d(y)
    }
    } function e(b) {
        var a = b.owner, c = a.state_;a = a.data_;
        var e = b[c];b = b.then;if ("function" === typeof e) {
        c = w;try {
            a = e(a)
        } catch (y) {
            l(b, y)
        }
    } d (b, a) || (c === w && g(b, a), c === r && l(b, a))
    } function d(b, a) {
        var c;try {
        if (b === a) throw new TypeError ("A promises callback cannot return that same promise.");if (a && ("function" ===
                    typeof a || "object" === typeof a)){
            var d = a.then;if ("function" === typeof d) return d.call(
            a,
            function(d) { c || (c = !0, a !== d?g(b, d):h(b, d)) },
            function(a) { c || (c = !0, l(b, a)) }), !0
        }
    } catch (y) {
        return c || l(b, y), !0
    }return !1
    } function g(b, a) { b !== a && d(b, a) || h(b, a) } function h(
        b,
        a
    ) { b.state_ === n && (b.state_ = q, b.data_ = a, f(v, b)) } function l(
        b,
        a
    ) { b.state_ === n && (b.state_ = q, b.data_ = a, f(u, b)) } function k(b) {
        var a = b.then_;b.then_ = void 0;for (b= 0;b < a.length;b++)e(a[b])
    } function v(b) { b.state_ = w;k(b) } function u(b) { b.state_ = r;k(b) } function m(b) {
        if ("function" !==
            typeof b
                ) throw new TypeError ("Promise constructor takes a function argument");if (!1 === this instanceof m) throw new TypeError ("Failed to construct 'Promise': Please use the 'new' operator, this object constructor cannot be called as a function.");this.then_ =
        [];c(b, this)
    }
    var p =
        b.Promise, t = p && "resolve" in p && "reject" in p && "all" in p && "race" in p && function(){
        var b;new p (function(
        a
    ) { b = a });return "function" === typeof b
    }();"undefined" !== typeof exports && exports ?(
        exports.Promise = t? p : m, exports.Polyfill = m):"function" ==
    typeof define && define.amd?define(function(){ return t?p:m }):t || (b.Promise = m);
    var n =
        "pending", q = "sealed", w = "fulfilled", r = "rejected", B = function(){}, A = "undefined" !== typeof setImmediate?setImmediate:setTimeout, x = [], z;m.prototype =
        {
            constructor:m, state_:n, then_:null, data_:void 0, then:function(b, a){
            b =
                { owner:this, then:new this.constructor(B), fulfilled:b, rejected:a };this.state_ === w || this.state_ === r?f(e, b):this.then_.push(b);return b.then
        }, "catch":function(b){ return this.then(null, b) }
        };m.all = function(b) {
        if ("[object Array]" !==
            Object.prototype.toString.call(b)
        ) throw new TypeError ("You must pass an array to Promise.all().");return new this(
        function(
            a,
            c
        ) {
            function d (b){
                f++;return function(c) {
                e[b] = c;--f || a(e)
            }
            }for (var e= [], f = 0, g = 0, h;g < b.length;g++)(h = b[g]) && "function" === typeof h.then?h.then(d(g), c):e[g] = h;f || a(
            e
        )
        })
    };m.race = function(b) {
        if ("[object Array]" !== Object.prototype.toString.call(b)) throw new TypeError ("You must pass an array to Promise.race().");return new this(
        function(a, c) {
            for (var d= 0, e;d < b.length;d++)(e = b[d]) && "function" ===
            typeof e.then?e.then(a, c):a(e)
        })
    };m.resolve = function(b) {
        return b && "object" === typeof b && b . constructor ===this?b:new this(function(a){
        a(b)
    })
    };m.reject = function(b) { return new this(function(a, c) { c(b) }) }
})("undefined"!=typeof window?window:"undefined"!=typeof global?global:"undefined"!=typeof self?self:this);
!function(b){ "object" == typeof exports &&"undefined" != typeof module?module.exports = b():"function" == typeof define && define.amd?define([], b):("undefined" != typeof window?window:"undefined" != typeof global?global:"undefined" != typeof self?self:this).store = b() }(function(){
    return function e (a, f, c){
        function d (h, k){
        if (!f[h]) {
            if (!a[h]) {
                var l = "function" == typeof require && require;if (!k && l) return l(
                    h,
                    !0
                );if (g) return g(h, !0);k = Error("Cannot find module '" + h + "'");throw k.code =
                    "MODULE_NOT_FOUND", k;
            } k = f [h] = { exports:{} };
            a[h][0].call(
                k.exports,
                function(c) { var e = a[h][1][c];return d(e? e : c) },
                k,
                k.exports,
                e,
                a,
                f,
                c
            )
        }return f[h].exports
    }for (var g= "function" == typeof require && require, h = 0;h < c.length;h++)d(c[h]);return d
    }({
        1:[function(a, f, c){
        c = a("../src/store-engine");
        var e = a("../storages/all");a = [a("../plugins/json2")];f.exports = c.createStore(e, a)
    },{ "../plugins/json2":2, "../src/store-engine":4, "../storages/all":6 }], 2:[function(a, f, c){
        f.exports = function() { return a("./lib/json2"),{} }
    },{ "./lib/json2":3 }], 3:[function(a, f, c){
        var e =
            "function" == typeof Symbol &&"symbol" == typeof Symbol.iterator?function(a){ return typeof a }:function(a){ return a && "function" == typeof Symbol && a . constructor === Symbol && a !== Symbol . prototype ?"symbol":typeof a };"object" !== ("undefined" == typeof JSON ?"undefined":e(JSON)) && (JSON ={});(function() {
        function a (a){ return 10 > a?"0"+a:a } function c() { return this.valueOf() } function f(a) {
            return p.lastIndex = 0, p.test(a)?'"'+a.replace(p, function(a){
            var c =
                w[a];return "string" == typeof c ? c :"\\u"+("0000"+a.charCodeAt(0).toString(16)).slice(-4)
        })+
            '"':'"'+a+'"'
        } function l(a, c) {
            var d, g, h, m = n, k = c[a];switch(
            k && "object" === ("undefined" == typeof k ?
            "undefined":e
            (k)
        ) && "function" == typeof k . toJSON &&(k = k.toJSON(a)), "function" == typeof r && (k = r.call(c, a, k)), "undefined" == typeof k?"undefined":e(k)){
            case "string":return f(k);case "number":return isFinite(k)?String(k):"null";case "boolean":case "null":return String(k);case "object":if(!k)return"null";if (n += q, h = [], "[object Array]" === Object.prototype.toString.apply(k)){
            c = k.length;for (a= 0;a < c;a += 1)h[a] = l(a, k) ||
            "null";return g =
            0 === h.length?"[]":n?"[\n"+n+h.join(",\n"+n)+"\n"+m+"]":"["+h.join(",")+"]", n = m, g
        }if (r && "object" === ("undefined" == typeof r ? "undefined":e(r)))for(c = r.length, a = 0;a < c;a += 1)"string" == typeof r[a] && (d = r[a], g = l(d, k), g && h.push(f(d)+(n?": ":":")+g));else for(d in k)Object.prototype.hasOwnProperty.call(k, d) && (g = l(d, k), g && h.push(f(d)+(n?": ":":")+g));return g =
            0 === h.length?"{}":n?"{\n"+n+h.join(",\n"+n)+"\n"+m+"}":"{"+h.join(",")+"}", n = m, g
        }
        }
        var k =/^[\], :{}\s]*$/, v = /\\( ?: ["\\\/bfnrt]|u[0-9a-fA-F]{4})/g,
        u =
                /"[^"\\\n\r]*"|true|false|null|-?\d+(?:\.\d*)?(?:[eE][+\-]?\d+)?/g,m=/(?:^|:|,)(?:\s*\[)+/g,p=/[\\"\u0000-\u001f\u007f-\u009f\u00ad\u0600-\u0604\u070f\u17b4\u17b5\u200c-\u200f\u2028-\u202f\u2060-\u206f\ufeff\ufff0-\uffff]/g, t = /[\u0000\u00ad\u0600-\u0604\u070f\u17b4\u17b5\u200c-\u200f\u2028-\u202f\u2060-\u206f\ufeff\ufff0-\uffff]/g;"function" != typeof Date . prototype . toJSON &&(
        Date.prototype.toJSON = function() {
            return isFinite(this.valueOf())?this.getUTCFullYear()+"-"+a(this.getUTCMonth()+1)+"-"+a(this.getUTCDate())+
            "T" + a(this.getUTCHours()) + ":" + a(this.getUTCMinutes()) + ":" + a(this.getUTCSeconds()) + "Z":null
        },
        Boolean.prototype.toJSON = c,
        Number.prototype.toJSON = c,
        String.prototype.toJSON = c
    );
        var n, q, w, r;"function" != typeof JSON . stringify &&(
        w = { "\b":"\\b", "\t":"\\t", "\n":"\\n", "\f":"\\f", "\r":"\\r", '"':'\\"', "\\":"\\\\" },
        JSON.stringify = function(a, c, d) {
            var f;if (n =
                "", q = "", "number" == typeof d)for(f = 0;f < d;f += 1)q += " ";else"string" == typeof d && (q = d);if (r =
                c, c && "function" != typeof c && ("object" !== ("undefined" == typeof c?"undefined":
            e(c)) || "number" != typeof c.length))throw Error("JSON.stringify");return l("",
            { "":a })
        });"function" != typeof JSON . parse &&(JSON.parse = function(a, c) {
        function d (a, f){
        var g, h, m = a[f];if (m && "object" === ("undefined" == typeof m ? "undefined":e(m)))for(g in m)Object.prototype.hasOwnProperty.call(m, g) && (h = d(m, g), void 0 !== h?m[g] = h:delete m[g]);return c.call(
        a,
        f,
        m
    )
    }
        var f;if (a = String(a), t.lastIndex = 0, t.test(a) && (a = a.replace(t, function(a){
        return "\\u" + ("0000" + a.charCodeAt(
            0
        ).toString(16)).slice(-4)
    })), k.test(a.replace(v,
        "@").replace(u, "]").replace(m, "")))return f = eval("("+a+")"), "function" == typeof c?d({ "":f }, ""):f;throw new SyntaxError ("JSON.parse");
    })
    })()
    },{}], 4:[function(a, f, c){
        function e (){
            var a =
                "undefined" == typeof console ?null:console;a && (a.warn?a.warn:a.log).apply(a, arguments)
        } function d(a, c, d) {
            d || (d = "");a && !u(a) && (a = [a]);c && !u(c) && (c = [c]);
            var f =
                d?"__storejs_"+d+"_":"", n = d?new RegExp("^"+f):null;if (!/^[a-zA-Z0-9_\-]*$/.test(d))throw Error("store.js namespaces can only have alphanumerics + underscores and dashes");
            var q = v({
                _namespacePrefix:f, _namespaceRegexp:n, _testStorage:function(a){
                try {
                    a.write("__storejs__test__", "__storejs__test__");
                    var c =
                        "__storejs__test__" === a.read("__storejs__test__");return a.remove("__storejs__test__"), c
                } catch (C) {
                    return !1
                }
            }, _assignPluginFnProp:function(a, c){
                var d = this[c];this[c] = function() {
                var c = g(arguments, 0), e = this, f = [function(){
                if (d) return l(
                    arguments,
                    function(a, d) { c[d] = a }), d.apply(e, c)
            }].concat(c);return a.apply(e, f)
            }
            }, _serialize:function(a){ return JSON.stringify(a) }, _deserialize:function(a,
                c){
                if (!a) return c;
                var d = "";try {
                d = JSON.parse(a)
            } catch (D) {
                d = a
            }return void 0 !== d?d:c
            }, _addStorage:function(a){
                this.enabled || this._testStorage(a) && (this.storage = a, this.enabled = !0)
            }, _addPlugin:function(a){
                var c = this;if (u(a)) return void l (a, function(a){ c._addPlugin(a) });if (!h(
                    this.plugins,
                    function(c) { return a === c })
            ) {
                if (this.plugins.push(a), !m(a))throw Error("Plugins must be function values that return objects");
                var d =
                    a.call(this);if (!p(d)) throw Error("Plugins must return an object of function properties");
                l(
                    d,
                    function(
                        d,
                        e
                    ) {
                        if (!m(d)) throw Error("Bad plugin property: " + e + " from plugin " + a.name + ". Plugins should only return functions.");c._assignPluginFnProp(
                        d,
                        e
                    )
                    })
            }
            }, addStorage:function(a){
                e("store.addStorage(storage) is deprecated. Use createStore([storages])");this._addStorage(
                a
            )
            }
            }, t, { plugins:[] });return q.raw = {}, l(q, function(a, c){
            m(a) && (q.raw[c] = k(q, a))
        }), l(a, function(a){ q._addStorage(a) }), l(c, function(a){ q._addPlugin(a) }), q
        } a = a ("./util");
        var g = a.slice, h = a.pluck, l = a.each, k = a.bind, v = a.create, u = a.isList,
        m = a.isFunction, p = a.isObject;f.exports = { createStore: d };
        var t = {
            version:"2.0.12", enabled:!1, get:function(a, c){
            a = this.storage.read(this._namespacePrefix + a);return this._deserialize(a, c)
        }, set:function(a, c){ return void 0 === c?this.remove(a):(this.storage.write(this._namespacePrefix+a, this._serialize(c)), c) }, remove:function(a){
            this.storage.remove(
                this._namespacePrefix + a
            )
        }, each:function(a){
            var c = this;this.storage.each(function(d, e) {
            a.call(
                c,
                c._deserialize(d),
                (e || "").replace(c._namespaceRegexp, "")
            )
        })
        }, clearAll:function(){ this.storage.clearAll() },
            hasNamespace:function(a){ return this._namespacePrefix == "__storejs_" + a + "_" }, createStore:function(){
            return d.apply(
                this,
                arguments
            )
        }, addPlugin:function(a){ this._addPlugin(a) }, namespace:function(a){
            return d(
                this.storage,
                this.plugins,
                a
            )
        }
        }
    },{ "./util":5 }], 5:[function(a, f, c){
        (function(a) {
            function c (a, c){ return Array.prototype.slice.call(a, c || 0) } function e(
            a,
            c
        ) { h(a, function(a, d) { return c(a, d), !1 }) } function h(a, c) {
            if (l(a)) for (var d= 0;d < a.length;d++){
            if (c(
                    a[d],
                    d
                )
            ) return a[d]
        }else for(d in a)if(a.hasOwnProperty(d) &&
            c(a[d], d))return a[d]
        } function l(a) { return null != a && "function" != typeof a &&"number" == typeof a.length }
            var k = function() {
                return Object.assign?Object.assign:function(a, c, d, f){
                for (var g= 1;g < arguments.length;g++)e(Object(arguments[g]), function(c, d){
                a[d] = c
            });return a
            }
            }(), v = function(){
            if (Object.create) return function(a, d, e, f) {
                var g = c(arguments, 1);return k.apply(this, [Object.create(a)].concat(g))
            };
            var a = function() {};return function(d, e, f, g) {
            var h = c(arguments, 1);return a.prototype = d, k.apply(this, [new a].concat(h))
        }
        }(),
            u = function() {
                return String.prototype.trim?function(a){
                return String.prototype.trim.call(a)
            }:function(a){ return a.replace(/^[\s\uFEFF\xA0]+|[\s\uFEFF\xA0]+$/g, "") }
            }();f.exports = {
            assign:k, create:v, trim:u, bind:function(a, c){
            return function() {
                return c.apply(
                    a,
                    Array.prototype.slice.call(arguments, 0)
                )
            }
        }, slice:c, each:e, map:function(a, c){
            var d = l(a)?[]:{};return h(
            a,
            function(a, e) { return d[e] = c(a, e), !1 }), d
        }, pluck:h, isList:l, isFunction:function(a){
            return a && "[object Function]" === {}.toString.call(
                a
            )
        }, isObject:function(a){
            return a &&
                    "[object Object]" === {}.toString.call(a)
        }, Global:"undefined" != typeof window?window:a
        }
        }).call(
            this,
            "undefined" != typeof global ? global :
            "undefined" != typeof self ? self :
            "undefined" != typeof window ? window :{})
    },{}], 6:[function(a, f, c){
        f.exports =
            [a("./localStorage"), a("./oldFF-globalStorage"), a("./oldIE-userDataStorage"), a("./cookieStorage"), a(
                "./sessionStorage"
            ), a("./memoryStorage")]
    },{ "./cookieStorage":7, "./localStorage":8, "./memoryStorage":9, "./oldFF-globalStorage":10, "./oldIE-userDataStorage":11, "./sessionStorage":12 }],
        7:[function(a, f, c){
        function e (a){
            for (var c= l.cookie.split(/; ?/g), d = c.length-1;0 <= d;d--)if(h(c[d])){
            var e = c[d].split("="), f = unescape(e[0]);e = unescape(e[1]);a(e, f)
        }
        } function d(a) {
            a && g(a) && (l.cookie = escape(a) + "=; expires=Thu, 01 Jan 1970 00:00:00 GMT; path=/")
        } function g(a) { return (new RegExp ("(?:^|;\\s*)" + escape(a).replace(/[\-\.\+\*]/g, "\\$&")+"\\s*\\=")).test(l.cookie) } a = a ("../src/util");c =
        a.Global;
        var h = a.trim;f.exports = {
        name:"cookieStorage", read:function(a){
        if (!a || !g(a)) return null;a = "(?:^|.*;\\s*)" +
            escape(a).replace(/[\-\.\+\*]/g, "\\$&")+"\\s*\\=\\s*((?:[^;](?!;))*[^;]?).*";return unescape(
        l.cookie.replace(new RegExp (a), "$1")
    )
    }, write:function(a, c){
        a && (l.cookie =
            escape(a) + "=" + escape(c) + "; expires=Tue, 19 Jan 2038 03:14:07 GMT; path=/")
    }, each:e, remove:d, clearAll:function(){ e(function(a, c) { d(c) }) }
    };
        var l = c.document
    },{ "../src/util":5 }], 8:[function(a, f, c){
        function e (a){ return d.localStorage.getItem(a) }
        var d = a("../src/util").Global;f.exports = {
        name:"localStorage", read:e, write:function(a, c){
        return d.localStorage.setItem(
            a,
            c
        )
    }, each:function(a){
        for (var c= d.localStorage.length - 1;0 <= c;c--){
        var f = d.localStorage.key(c);a(e(f), f)
    }
    }, remove:function(a){ return d.localStorage.removeItem(a) }, clearAll:function(){ return d.localStorage.clear() }
    }
    },{ "../src/util":5 }], 9:[function(a, f, c){
        f.exports = {
            name:"memoryStorage", read:function(a){ return e[a] }, write:function(a, c){
            e[a] = c
        }, each:function(a){
            for (var c in e) e.hasOwnProperty(c) && a(
                e[c],
                c
            )
        }, remove:function(a){ delete e [a] }, clearAll:function(a){ e = {} }
        };
        var e = {}
    },{}], 10:[function(a, f, c){
        function e (a){
            for (var c=
                d.length - 1;0 <= c;c--){ var e = d.key(c);a(d[e], e) }
        } a = a ("../src/util").Global;f.exports = {
        name:"oldFF-globalStorage", read:function(a){ return d[a] }, write:function(a, c){
        d[a] = c
    }, each:e, remove:function(a){ return d.removeItem(a) }, clearAll:function(){
        e(
            function(
                a,
                c
            ) { delete d [a] })
    }
    };
        var d = a.globalStorage
    },{ "../src/util":5 }], 11:[function(a, f, c){
        function e (a){ return a.replace(/^\d/, "___$&").replace(k, "___") } a = a ("../src/util").Global;f.exports =
        {
            name:"oldIE-userDataStorage", write:function(a, c){
            if (!l) {
                var f = e(a);h(function(a) {
                    a.setAttribute(
                        f,
                        c
                    );a.save(d)
                })
            }
        }, read:function(a){
            if (!l) {
                var c = e(a), d = null;return h(function(a) { d = a.getAttribute(c) }), d
            }
        }, each:function(a){
            h(function(c) {
                for (var d= c.XMLDocument.documentElement.attributes, e = d.length-1;0 <= e;e--){
                var f = d[e];a(c.getAttribute(f.name), f.name)
            }
            })
        }, remove:function(a){
            var c = e(a);h(function(a) { a.removeAttribute(c);a.save(d) })
        }, clearAll:function(){
            h(function(a) {
                var c =
                    a.XMLDocument.documentElement.attributes;a.load(d);for (var e= c.length - 1;0 <= e;e--)a.removeAttribute(c[e].name);a.save(
                d
            )
            })
        }
        };
        var d = "storejs", g = a.document, h = function(){
        if (!g || !g.documentElement || !g.documentElement.addBehavior) return null;try {
        var a = new ActiveXObject ("htmlfile");a.open();a.write(
            '<script>document.w=window\x3c/script><iframe src="/favicon.ico"></iframe>'
        );a.close();
        var c = a.w.frames[0].document;
        var e = c.createElement("div")
    } catch (p) {
        e = g.createElement("div"), c = g.body
    }return function(a) {
        var f = [].slice.call(
            arguments,
            0
        );f.unshift(e);c.appendChild(e);e.addBehavior("#default#userData");e.load(d);a.apply(
        this,
        f
    );c.removeChild(e)
    }
    }(),
        l =
            (a.navigator?a.navigator.userAgent:"").match(/ (MSIE 8|MSIE 9|MSIE 10)\./), k = /[!"#$%&'()*+,/\\:;<=>?@[\]^`{|}~]/g},{"../src/util":5}],12:[function(a,f,c){function e(a){return d.sessionStorage.getItem(a)}var d=a("../src/util").Global;f.exports={name:"sessionStorage",read:e,write:function(a,c){return d.sessionStorage.setItem(a,c)},each:function(a){for(var c=d.sessionStorage.length-1;0<=c;c--){var f=d.sessionStorage.key(c);a(e(f),f)}},remove:function(a){return d.sessionStorage.removeItem(a)},clearAll:function(){return d.sessionStorage.clear()}}},
        { "../src/util":5 }]
    },{}, [1])(1)
    });function rheaLog (b){ console.log(b) } function RheaAjaxAnswer(b) {
        this.requestID = b;this.rcv = null
    } function rheaAddU16ToUint8Buffer(b, a, f) {
        f = parseInt(f);b[a] = (f&65280)>>8;b[a + 1] = f&255
    } function rheaAddU32ToUint8Buffer(b, a, f) {
        f = parseInt(f);b[a] = (f&4278190080)>>24;b[a + 1] = (f&16711680)>>16;b[a + 2] =
        (f&65280)>>8;b[a + 3] = f&255
    } function rheaReadU32FromUint8Buffer(
        b,
        a
    ) { return parseInt(b[a]) < <24|parseInt(b[a+1])<<16|parseInt(b[a+2])<<8|parseInt(b[a+3]) }
    function rheaReadU16FromUint8Buffer (b, a){ return parseInt(b[a]) < <8|parseInt(b[a+1]) } function rheaSetCookie(
        b,
        a,
        f
    ) {
        var c = "";f && (c =
        new Date, c.setTime(c.getTime()+864E5*f), c = "; expires="+c.toUTCString());document.cookie =
        b + "=" + (a || "") + c + "; path=/";console.log("SET COOKIE [" + b + "][" + a + "][" + f + "]")
    }
    function rheaGetCookie (b){
        b += "=";for (var a= document.cookie.split(";"), f = 0;f < a.length;f++){
        for (var c= a[f];" " == c.charAt(
        0
    );)c = c.substring(1, c.length);if (0 == c.indexOf(b)) return c.substring(b.length, c.length)
    }return null
    } function rheaGetCookieOrDefault(b, a) {
        b = rheaGetCookie(b);return null == b?a:b
    } function rheaEraseCookie(b) { document.cookie = b + "=; Max-Age=-99999999;" }
    function rheaGetAbsolutePhysicalPath (){
        var b = window.location.pathname.split("/").slice(0, -1).join("/");"/" == b.substr(
        0,
        1
    ) && ":" == b.substr(2, 1) && (b = b.substr(1));return b
    }
    var utf8ArrayToStr = function() {
        var b = Array(128), a = String.fromCodePoint || String.fromCharCode, f = [];return function(
        c
    ) {
        for (var e, d = c.length, g = f.length = 0;g < d;){
        e =
            c[g++];127 >= e || (223 >= e?e = (e&31)<<6|c[g++]&63:239>=e?e = (e&15)<<12|(c[g++]&63)<<6|c[g++]&63:String.fromCodePoint?e = (e&7)<<18|(c[g++]&63)<<12|(c[g++]&63)<<6|c[g++]&63:(e = 63, g += 3));try {
        f.push(b[e] || (b[e] = a(e)))
    } catch (h) {
        f.push("\u00a7")
    }
    }return f.join("")
    }
    }(), utf16ArrayToStr = function(){
        var b = Array(128), a = String.fromCodePoint || String.fromCharCode,
        f = [];return function(c) {
        for (var e, d = c.length, g = f.length = 0;g < d;){
        e = c[g++];
        var h = c[g++];e| = h<<8;f.push(b[e] || (b[e] = a(e)))
    }return f.join("")
    }
    }();function utf16StrToStr (b, a){
        for (var f= Array(128), c = String.fromCodePoint || String.fromCharCode, e = [];;){
        var d =
            b.charCodeAt(a++), g = b.charCodeAt(a++);console.log("[" + d + "][" + g + "]");if (60 < a) break;if (0 == d && 0 == g) return e.join(
        ""
    );d| = g<<8;e.push(f[d] || (f[d] = c(d)))
    }
    }
    function rheaLoadScript (b){
        return new Promise (function(a, f) {
            var c = document.createElement("script");c.src = b;c.onload = a;c.onerror =
            f;document.head.appendChild(c)
        })
    } function rheaGetElemByID(b) {
        b = document.getElementById(b);if (null !== b && void 0 !== b)return b
    } function rheaDoesElemExistsByID(b) {
        b = document.getElementById(b);if (null !== b && void 0 !== b)return b
    } function rheaSetDivHTMLByName(b, a) {
        rheaGetElemByID(b).innerHTML = a
    } function _rheaGetElemProp(b) { return void 0 === b || "" == b?0:parseInt(b) }
    function rheaGetElemWidth (b){ return _rheaGetElemProp(b.offsetWidth) } function rheaGetElemHeight(
        b
    ) { return _rheaGetElemProp(b.offsetHeight) } function rheaGetElemLeft(b) {
        return _rheaGetElemProp(
            b.style.left
        )
    } function rheaGetElemTop(b) { return _rheaGetElemProp(b.style.top) } function rheaGetElemHTML(b) { return b.innerHTML } function rheaSetElemHTML(
        b,
        a
    ) { b.innerHTML = a } function rheaSetElemWidth(b, a) {
        b.style.width = a + "px"
    } function rheaSetElemHeight(b, a) { b.style.height = a + "px" }
    function rheaSetElemLeft (b, a){ b.style.left = a + "px" } function rheaSetElemTop(
        b,
        a
    ) { b.style.top = a + "px" } function rheaSetDisplayMode(b, a) {
        b.style.display = a
    } function rheaSetElemHREF(b, a) {
        b.setAttribute(
            "href",
            a
        )
    } function rheaSetElemBackgroundImage(b, a) {
        b.style.backgroundImage = "url(" + a + ")"
    } function rheaSetElemBackgroundColor(b, a) {
        b.style.backgroundColor = a
    } function rheaSetElemTextColor(b, a) { b.style.color = a } function rheaAddClassToElem(
        b,
        a
    ) { b.classList.add(a) }
    function rheaRemoveClassToElem (b, a){ b.classList.remove(a) } function rheaHideElem(b) {
        rheaSetDisplayMode(
            b,
            "none"
        )
    } function rheaShowElem(b) {
        rheaSetDisplayMode(
            b,
            "block"
        )
    } function rheaShowElem_TABLE(b) {
        rheaSetDisplayMode(
            b,
            "table"
        )
    } function rheaShowElem_TR(b) {
        rheaSetDisplayMode(
            b,
            "table-row"
        )
    } function rheaShowElem_TD(b) {
        rheaSetDisplayMode(
            b,
            "table-column"
        )
    } function rheaEase_linear(b) { return b } function rheaEase_outCubic(b) { return --b * b * b + 1 }
    function rheaSmoothScrollElemLeft (b, a, f){
        var c = rheaGetElemLeft(b), e = a-c, d = 0, g = setInterval(function(){
        d += 20;
        var a =
            d / f;1 <= a?(clearInterval(g), rheaSetElemLeft(b, c+e)):(a = rheaEase_outCubic(a), val = c+e*a, rheaSetElemLeft(b, val ))
    }, 20);return g
    } function rheaSmoothScrollElemTop(b, a, f) {
        var c = rheaGetElemTop(b), e = a-c, d = 0, g = setInterval(function(){
        d += 20;
        var a =
            d / f;1 <= a?(clearInterval(g), rheaSetElemTop(b, c+e)):(a = rheaEase_outCubic(a), val = c+e*a, rheaSetElemTop(b, val ))
    }, 20);return g
    }
    function rheaGetURLParamOrDefault (b, a){
        for (var f= location.search.substr(1).split("&"), c = 0;c < f.length;c++){
        var e = f[c].split("=");if (e[0] === b) return decodeURIComponent(e[1])
    }return a
    } function ObjRheaScrollDivByGestureInfo(b, a, f) {
        this.divContent = b;this.mouse_y = this.mouse_pressed = 0;this.totalH =
        rheaGetElemHeight(b);this.scroll_miny = -(a - f);this.scroll_howMuch =
        f / 2;this.scroll_tollerance_at_border = 5
    }
    function rheaSetDivAsScrollabelByGesture (b, a, f, c){
        if (!(f <= c)) {
            a = rheaGetElemByID(a);
            var e =
                "<div id='divArrowUp'   class='bigScrollArrowUp'   style='top:0; display:none'><center><img draggable='false' src='img/big-arrow-up.png' height='30'></center></div><div id='divArrowDown' class='bigScrollArrowDown' style='top:" + (c - 40 + "px; display:none'><center><img draggable='false' style='margin-top:11px' src='img/big-arrow-down.png' height='30'></center></div>");rheaSetElemHTML(
                a,
                rheaGetElemHTML(a) + e
            );rheaShowElem(rheaGetElemByID("divArrowDown"));
            b = rheaGetElemByID(b);
            var d = new ObjRheaScrollDivByGestureInfo (b, f, c);b.addEventListener(
                "mousedown",
                function(a) { d.mouse_pressed = 1;d.mouse_y = a.clientY },
                !0
            );b.addEventListener(
                "mouseup",
                function(a) { d.mouse_pressed = 0 },
                !0
            );b.addEventListener("mousemove", function(a) {
                if (d.mouse_pressed) {
                    var b = a.clientY;a = b - d.mouse_y;d.mouse_y = b;b =
                        rheaGetElemTop(d.divContent);b += a;0 <= b && (b =
                        0);b < d.scroll_miny && (b = d.scroll_miny);rheaSetElemTop(
                        d.divContent,
                        b
                    );b < -d.scroll_tollerance_at_border?rheaShowElem(rheaGetElemByID("divArrowUp")):
                    rheaHideElem(rheaGetElemByID("divArrowUp"));b < d.scroll_miny + d.scroll_tollerance_at_border?rheaHideElem(rheaGetElemByID("divArrowDown")):rheaShowElem(rheaGetElemByID("divArrowDown"))
                }
            }, !0);b = rheaGetElemByID("divArrowDown");b.addEventListener("click", function(a) {
                a =
                    rheaGetElemTop(d.divContent);a -= d.scroll_howMuch;a <= d.scroll_miny + d.scroll_tollerance_at_border && (a =
                d.scroll_miny, rheaHideElem(rheaGetElemByID("divArrowDown")));rheaShowElem(
                rheaGetElemByID("divArrowUp")
            );rheaSmoothScrollElemTop(
                d.divContent,
                a, 300
            )
            }, !0);b = rheaGetElemByID("divArrowUp");b.addEventListener(
                "click",
                function(a) {
                    a =
                        rheaGetElemTop(d.divContent);a += d.scroll_howMuch;a >= -d.scroll_tollerance_at_border && (rheaHideElem(
                    rheaGetElemByID("divArrowUp")
                ), a = 0);rheaShowElem(rheaGetElemByID("divArrowDown"));rheaSmoothScrollElemTop(
                    d.divContent,
                    a,
                    300
                )
                },
                !0
            )
        }
    } function rheaLeftPad(b, a, f) { for (b= b.toString();b.length < a;)b = f + b;return b }
    var RHEA_DEFAULT_FALLOFF_LANGUAGE =
        "GB", RHEA_NUM_MAX_SELECTIONS = 48, RHEA_EVENT_SELECTION_AVAILABILITY_UPDATED = 97, RHEA_EVENT_SELECTION_PRICES_UPDATED = 98, RHEA_EVENT_CREDIT_UPDATED = 99, RHEA_EVENT_CPU_MESSAGE = 100, RHEA_EVENT_SELECTION_REQ_STATUS = 101, RHEA_EVENT_START_SELECTION = 102, RHEA_EVENT_STOP_SELECTION = 103, RHEA_EVENT_CPU_STATUS = 104, RHEA_EVENT_ANSWER_TO_IDCODE_REQUEST = 105, RHEA_EVENT_SEND_BUTTON = 115, RHEA_EVENT_SEND_PARTIAL_DA3 = 116, RHEA_EVENT_READ_DATA_AUDIT = 108, RHEA_EVENT_CPU_ACTIVATE_BUZZER = 71, RHEA_CLIENT_INFO__API_VERSION =
    1, RHEA_CLIENT_INFO__APP_TYPE = 1, RHEA_CLIENT_INFO__UNUSED2 = 0, RHEA_CLIENT_INFO__UNUSED3 = 0;Rhea_clearSessionData =
        function(b) {
            window.name = "";Rhea_session_setValue(
            "lang",
            RHEA_DEFAULT_FALLOFF_LANGUAGE
        );Rhea_session_setValue("credit", "0");Rhea_session_setValue(
            "debug",
            0
        );Rhea_session_setValue(
            "debug_console",
            ""
        );for (b= 1;b <= RHEA_NUM_MAX_SELECTIONS;b++)Rhea_session_clearObject("selInfo"+b)
        };
    function Rhea (){
        void 0 === Rhea_session_getValue("lang") && Rhea_session_setValue("lang", RHEA_DEFAULT_FALLOFF_LANGUAGE);"" == window.name?this.idCode_3 = this.idCode_2 = this.idCode_1 = this.idCode_0 = 0:(this.idCode_0 = parseInt(window.name.substr(4, 3)), this.idCode_1 = parseInt(window.name.substr(8, 3)), this.idCode_2 = parseInt(window.name.substr(12, 3)), this.idCode_3 = parseInt(window.name.substr(16, 3)));this.nextAjaxRequestID =
        1;this.ajaxReceivedAnswerQ =
        [];for (var b= 0;16 > b;b++)this.ajaxReceivedAnswerQ[b] = null;this.selection_sessionRestore();
        Rhea_session_getValue("credit")?this.credit = Rhea_session_getValue("credit"):(this.credit = "0", Rhea_session_setValue("credit", "0"));this.nFileTransfer =
        0;this.fileTransfer = [];this.partialDA3AckRcvd = -1
    }
    Rhea.prototype.webSocket_connect = function() {
        var b = this;return new Promise (function(a, f) {
        rheaLog("Rhea:: trying to connect...");b.websocket =
        new WebSocket ("ws://127.0.0.1:2280/", "binary");b.websocket.onopen = function(c) {
        rheaLog("Rhea::webSocket connected...");if (0 == b.idCode_0) {
        b.webSocket_requestIDCodeAfterConnection();
        var e =
            2E3, d = function(){ 0 != b.idCode_3?(b.webSocket_identifyAfterConnection(), a(1)):0>(e -= 100)?(f("timed out 'waiting idcode"), f(0)):setTimeout(d, 100) };setTimeout(
            d,
            100
        )
    } else b.webSocket_identifyAfterConnection(),
        a(1)
    };b.websocket.onclose = function(a) { b.webSocket_onClose(a) };b.websocket.onmessage =
        function(a) { b.webSocket_onRcv(a) };b.websocket.onerror = function(a) {
        rheaLog("Rhea::onWebSocketErr => ERROR: " + a.data);setTimeout(function() {
        window.location = "startup.html"
    }, 2E3)
    };setTimeout(function() { f(-1) }, 5E3)
    })
    };Rhea.prototype.webSocket_onClose =
        function(b) { rheaLog("Rhea::webSocket_onClose =>  Disconnected");this.websocket.close() };
    Rhea.prototype.priv_findFileTransferByAppUID =
        function(b) { for (var a= 0;a < this.nFileTransfer;a++)if(null != this.fileTransfer[a] && this.fileTransfer[a].appTransfUID == b)return a;return -1 };
    Rhea.prototype.webSocket_onRcv = function(b) {
        var a = this, f = new FileReader;f.readAsArrayBuffer(b.data);f.onload = function(b) {
        b =
            new Uint8Array (f.result);if (6 < b.length && 35 == b[0] && 65 == b[1] && 74 == b[2] && 106 == b[4] && 97 == b[5]) for (var c= parseInt(
        b[3]
    ), d = 0;d < a.ajaxReceivedAnswerQ.length;d++){
        if (null != a.ajaxReceivedAnswerQ[d] && a.ajaxReceivedAnswerQ[d].requestID == c) {
            a.ajaxReceivedAnswerQ[d].rcv =
                11 < b.length?1 == b[6] && 2 == b[7] && 3 == b[8] && 4 == b[9] && 5 == b[10]?utf16ArrayToStr(b.subarray(11)):utf8ArrayToStr(b.subarray(6)):utf8ArrayToStr(b.subarray(6));
            break
        }
    }else if(5<=b.length && 35 == b[0] && 101 == b[1] && 86 == b[2] && 110 == b[3])switch(d = parseInt(b[4]), parseInt(b[5]), parseInt(256*b[6]), parseInt(b[7]), d){
        case RHEA_EVENT_ANSWER_TO_IDCODE_REQUEST :if (0 == a.idCode_0) {
        a.idCode_0 = parseInt(b[10]);a.idCode_1 = parseInt(b[11]);a.idCode_2 =
            parseInt(b[12]);a.idCode_3 =
            parseInt(b[13]);for (b= a.idCode_0.toString();3 > b.length;)b =
            "0" + b;for (d= a.idCode_1.toString();3 > d.length;)d =
            "0" + d;for (c= a.idCode_2.toString();3 > c.length;)c =
            "0" + c;for (var g= a.idCode_3.toString();3 > g.length;)g =
            "0" + g;window.name = "rhea" + b + "-" + d + "-" + c + "-" + g
    }break;case RHEA_EVENT_SELECTION_AVAILABILITY_UPDATED : rheaLog ("RHEA_EVENT_SELECTION_AVAILABILITY_UPDATED:");c =
        parseInt(b[8]);for (d= 1;d <= c;d++)a.selection_getBySelNumber(d).enabled = parseInt(b[8+d]);a.selection_sessionStore();a.onEvent_selectionAvailabilityUpdated();break;case RHEA_EVENT_SELECTION_PRICES_UPDATED : rheaLog ("RHEA_EVENT_SELECTION_PRICES_UPDATED:");b =
        utf8ArrayToStr(b.subarray(8)).split("\u00a7");for (d= 1;d <= b.length;d++)a.selection_getBySelNumber(d).price =
        b[d - 1];a.selection_sessionStore();a.onEvent_selectionPricesUpdated();break;case RHEA_EVENT_CREDIT_UPDATED : rheaLog ("RHEA_EVENT_CREDIT_UPDATED:");a.credit =
        utf8ArrayToStr(b.subarray(8));Rhea_session_setValue(
        "credit",
        a.credit
    );a.onEvent_creditUpdated();break;case RHEA_EVENT_CPU_MESSAGE : d = b [8];b =
        utf16ArrayToStr(b.subarray(11));if ("" != b) a.onEvent_cpuMessage(
        b,
        d
    );break;case RHEA_EVENT_SELECTION_REQ_STATUS : b = parseInt (b[8]);rheaLog("RHEA_EVENT_SELECTION_REQ_STATUS [" + b + "]");a.onEvent_selectionReqStatus(
        b
    );
        break;case RHEA_EVENT_CPU_STATUS : d = parseInt (b[8]);b = parseInt(256 * b[11] + b[12]);c =
        "";switch(d) {
        case 2:c = "READY";break;case 3:c = "DRINK PREP";break;case 4:c = "PROGR";break;case 5:c = "INI CHECK";break;case 6:c = "ERROR";break;case 7:c = "RINSING";break;case 8:c = "AUTO WASHING";break;case 10:c = "WAIT TEMP";break;case 13:c = "DISINSTALLATION";break;case 15:c = "END DISINSTALLATION";break;case 20:c = "BREWER CLEANING";break;case 21:c = "TEST SEL";break;case 22:c = "TEST MODEM";break;case 23:c = "CLEANING MILKER";break;case 24:c =
        "CLEANING MILKER";break;case 26:c = "DESCALING";break;case 101:c = "COM_ERROR";break;case 102:c = "GRINDER OPENING";break;case 103:c = "COMPATIBILITY CHECK";break;case 104:c = "CPU_NOT_SUPPORTED";break;case 105:c = "DA3_SYNC";break;case 106:c = "GRINDER SPEED TEST"
    } a . onEvent_cpuStatus (d, c, b);break;case RHEA_EVENT_SEND_PARTIAL_DA3 : a . partialDA3AckRcvd = parseInt (b[8]);break;case RHEA_EVENT_READ_DATA_AUDIT : fileID =256*b[8]+b[9], kbSoFar = 256*b[10]+b[11], b = b[12], a.onEvent_readDataAudit(b, kbSoFar, fileID)
    }else{
        if (9 <= b.length &&
            35 == b[0] && 102 == b[1] && 84 == b[2] && 114 == b[3]
        ) switch(parseInt(b[4]), parseInt(b[5]), d = parseInt(b[6]), d) {
            case 2:c ={};c.reason_refused = parseInt(b[7]);c.packetSizeInBytes =
            256 * parseInt(b[8]) + parseInt(b[9]);c.smuTransfUID =
            parseInt(b[10]) < <24|parseInt(b[11])<<16|parseInt(b[12])<<8|parseInt(b[13]);c.appTransfUID =
            parseInt(b[14]) < <24|parseInt(b[15])<<16|parseInt(b[16])<<8|parseInt(b[17]);c.numPacketInAChunk =
            parseInt(b[18]);for (d= 0;d < a.nFileTransfer;d++)if(a.fileTransfer[d].appTransfUID == c.appTransfUID){
            a.fileTransfer[d].priv_on0x02(c) ||
                    (a.fileTransfer[d] = null);return
        } rheaLog ("ERR, file transfer not found. sData0x02.appTransfUID[" + ssData0x02.appTransfUID + "] sData0x02.smuTransfUID[" + c.smuTransfUID + "]");return;case 4:c ={};c.appTransfUID =
            parseInt(b[7]) < <24|parseInt(b[8])<<16|parseInt(b[9])<<8|parseInt(b[10]);c.packetNumAccepted =
            parseInt(b[11]) < <24|parseInt(b[12])<<16|parseInt(b[13])<<8|parseInt(b[14]);for (d= 0;d < a.nFileTransfer;d++)if(a.fileTransfer[d].appTransfUID == c.appTransfUID){
            a.fileTransfer[d].priv_on0x04(c) || (a.fileTransfer[d] =
                null);return
        } rheaLog ("ERR, file transfer not found. sData0x04.appTransfUID[" + c.appTransfUID + "] sData0x04.packetNumAccepted[" + c.packetNumAccepted + "]");return;case 82:c ={};c.reason_refused =
            parseInt(b[7]);c.packetSizeInBytes =
            256 * parseInt(b[8]) + parseInt(b[9]);c.smuTransfUID =
            parseInt(b[10]) < <24|parseInt(b[11])<<16|parseInt(b[12])<<8|parseInt(b[13]);c.appTransfUID =
            parseInt(b[14]) < <24|parseInt(b[15])<<16|parseInt(b[16])<<8|parseInt(b[17]);c.fileSize =
            parseInt(b[18]) < <24|parseInt(b[19])<<16|parseInt(b[20])<<
            8|parseInt(b[21]);c.numPacketInAChunk = parseInt(b[22]);d =
            a.priv_findFileTransferByAppUID(c.appTransfUID);if (0 <= d) {
            a.fileTransfer[d].priv_on0x52(c) || (a.fileTransfer[d] = null);return
        } rheaLog ("ERR, file transfer not found. sData0x52.appTransfUID[" + ssData0x52.appTransfUID + "] sData0x02.smuTransfUID[" + c.smuTransfUID + "]");return;case 83:c ={};c.appTransfUID =
            parseInt(b[7]) < <24|parseInt(b[8])<<16|parseInt(b[9])<<8|parseInt(b[10]);c.packetNumReceived =
            parseInt(b[11]) < <24|parseInt(b[12])<<16|parseInt(b[13])<<8|
            parseInt(b[14]);c.chunkSeq = parseInt(b[15]);d =
            a.priv_findFileTransferByAppUID(c.appTransfUID);if (0 <= d) {
            c =
                a.fileTransfer[d].priv_on0x53(c);if (0 < c) for (g= 0;g < c;g++)a.fileTransfer[d].fileBuffer[a.fileTransfer[d].fileBufferCT++] = b[16+g];else if(0>c){
                c =
                    -c;for (g= 0;g < c;g++)a.fileTransfer[d].fileBuffer[a.fileTransfer[d].fileBufferCT++] = b[16+g];a.fileTransfer[d].callback_onEnd(
                a.fileTransfer[d].userValue,
                0,
                a.fileTransfer[d]
            );a.fileTransfer[d] = null
            }return
        } rheaLog ("ERR, file transfer not found. sData0x53.appTransfUID[" +
                c.appTransfUID + "] sData0x53.packetNumReceived[" + c.packetNumReceived + "]");return;default:rheaLog("#fTr, opcode["+d+"]");return
        } rheaLog ("Rhea::RCV [len=" + b.length + "] [" + utf8ArrayToStr(b) + "]")
    }
    }
    };Rhea.prototype.sendBinary =
        function(b, a, f) { new DataView (b.buffer, a, f);this.websocket.send(b) };
    Rhea.prototype.sendGPUCommand = function(b, a, f, c) {
        var e = 255, d;if (f) {
        e = this.nextAjaxRequestID++;200 < this.nextAjaxRequestID && (this.nextAjaxRequestID =
            1);for (d= 0;d < this.ajaxReceivedAnswerQ.length;d++)if(null == this.ajaxReceivedAnswerQ[d]){
            this.ajaxReceivedAnswerQ[d] = new RheaAjaxAnswer (e);break
        }if (d >= this.ajaxReceivedAnswerQ.length) return rheaLog("Rhea::GPUCommand => too many request"), new Promise(function(a, b){
            b(
                "Rhea::GPUCommand => too many request"
            )
        })
    }
        var g = 0;null != a && (g = a.length);
        var h = new Uint8Array (5 +
                g + 2), l = 0;h[l++] = 35;h[l++] = b.charCodeAt(0);h[l++] = e;h[l++] =
        (g&65280)>>8;h[l++] = g&255;for (b= 0;b < g;b++)h[l++] = a[b];for (b= a =
        0;b < l;b++)a += h[b];h[l++] = (a&65280)>>8;h[l++] = a&255;this.sendBinary(
        h,
        0,
        l
    );if (0 != f) {
        var k = this;return new Promise (function(a, b) {
            var e =
                c, f = function(){ null != k.ajaxReceivedAnswerQ[d].rcv?(a(k.ajaxReceivedAnswerQ[d].rcv), k.ajaxReceivedAnswerQ[d] = null):0>(e -= 25)?(b("timed out [reqID:"+k.ajaxReceivedAnswerQ[d].requestID+"][pos:"+d+"]"), k.ajaxReceivedAnswerQ[d] = null):setTimeout(f, 25) };setTimeout(
            f,
            25
        )
        })
    }
    };Rhea.prototype.ajax = function(b, a) {
        return this.ajaxWithCustomTimeout(
            b,
            a,
            4E3
        )
    };Rhea.prototype.ajaxWithCustomTimeout = function(b, a, f) {
        "" == a && (a = "{}");a = JSON.stringify(a);
        var c = new Uint8Array (1 + b.length + 2 + a.length), e = 0;c[e++] =
        b.length&255;for (var d= 0;d < b.length;d++)c[e++] = b.charCodeAt(d);c[e++] =
        (a.length&65280)>>8;c[e++] =
        a.length&255;for (d= 0;d < a.length;d++)c[e++] = a.charCodeAt(d);return this.sendGPUCommand(
        "A",
        c,
        1,
        f
    )
    };
    Rhea.prototype.requestGPUEvent = function(b) {
        var a = new Uint8Array (1);a[0] = b;this.sendGPUCommand(
        "E",
        a,
        0,
        0
    )
    };Rhea.prototype.requestGPUCleaning = function(b) {
        this.sendCPUProgrammingCmd(
            2,
            b,
            0,
            0,
            0
        )
    };Rhea.prototype.sendCPUProgrammingCmd = function(b, a, f, c, e) {
        var d = new Uint8Array (6);d[0] = 113;d[1] = parseInt(b);d[2] = parseInt(a);d[3] =
        parseInt(f);d[4] = parseInt(c);d[5] = parseInt(e);this.sendGPUCommand("E", d, 0, 0)
    };
    Rhea.prototype.sendButtonPress = function(b) {
        var a = new Uint8Array (2);a[0] = RHEA_EVENT_SEND_BUTTON;a[1] =
        parseInt(b);this.sendGPUCommand("E", a, 0, 0)
    };Rhea.prototype.sendStartDisintallation = function() {
        var b = new Uint8Array (1);b[0] = 65;this.sendGPUCommand(
        "E",
        b,
        0,
        0
    )
    };Rhea.prototype.sendSyncDA3 = function() {
        var b = new Uint8Array (1);b[0] = 67;this.sendGPUCommand(
        "E",
        b,
        0,
        0
    )
    };Rhea.prototype.sendGetCPUStatus = function() {
        var b = new Uint8Array (1);b[0] = RHEA_EVENT_CPU_STATUS;this.sendGPUCommand("E", b, 0, 0)
    };
    Rhea.prototype.sendStartDownloadDataAudit = function() {
        var b = new Uint8Array (1);b[0] = RHEA_EVENT_READ_DATA_AUDIT;this.sendGPUCommand(
        "E",
        b,
        0,
        0
    )
    };Rhea.prototype.sendStartPosizionamentoMacina = function(b, a) {
        var f = new Uint8Array (4);f[0] = 122;f[1] = parseInt(b);f[2] = (a&65280)>>8;f[3] =
        a&255;this.sendGPUCommand("E", f, 0, 0)
    };
    Rhea.prototype.sendPartialDA3AndReturnAPromise = function(b, a, f, c, e) {
        var d = new Uint8Array (68);d[0] =
        RHEA_EVENT_SEND_PARTIAL_DA3;for (var g= 0;64 > g;g++)d[g+1] = c[e+g];d[65] =
        parseInt(b);d[66] = parseInt(a);d[67] = parseInt(f);this.sendGPUCommand(
        "E",
        d,
        0,
        0
    );this.partialDA3AckRcvd = -1;
        var h = this;return new Promise (function(a, b) {
        var c =
            5E3, d = function(){ h.partialDA3AckRcvd == f?a("OK"):0>(c -= 250)?a("KO"):setTimeout(d, 250) };setTimeout(
        d,
        250
    )
    })
    };
    Rhea.prototype.webSocket_requestIDCodeAfterConnection = function() {
        rheaLog("Rhea::webSocket requesting idCode");
        var b = new Uint8Array (4);b[0] = RHEA_CLIENT_INFO__API_VERSION;b[1] =
        RHEA_CLIENT_INFO__APP_TYPE;b[2] = RHEA_CLIENT_INFO__UNUSED2;b[3] =
        RHEA_CLIENT_INFO__UNUSED3;this.sendGPUCommand("I", b, 0, 0)
    };
    Rhea.prototype.webSocket_identifyAfterConnection = function() {
        rheaLog("Rhea::webSocket sending idCode[" + this.idCode_0 + "][" + this.idCode_1 + "][" + this.idCode_2 + "][" + this.idCode_3 + "]");
        var b = new Uint8Array (8);b[0] = RHEA_CLIENT_INFO__API_VERSION;b[1] =
        RHEA_CLIENT_INFO__APP_TYPE;b[2] = RHEA_CLIENT_INFO__UNUSED2;b[3] =
        RHEA_CLIENT_INFO__UNUSED3;b[4] = this.idCode_0;b[5] = this.idCode_1;b[6] =
        this.idCode_2;b[7] = this.idCode_3;this.sendGPUCommand("W", b, 0, 0)
    };
    Rhea.prototype.filetransfer_startUpload = function(
        b,
        a,
        f,
        c,
        e,
        d,
        g
    ) {
        for (var h= 0;h < this.nFileTransfer;h++)if(null == this.fileTransfer[h]){
        this.fileTransfer[h] = new RheaFileUpload (b, a, f, c, e, d, g);return
    }this.fileTransfer[this.nFileTransfer++] = new RheaFileUpload (b, a, f, c, e, d, g)
    };
    Rhea.prototype.filetransfer_startDownload = function(
        b,
        a,
        f,
        c,
        e
    ) {
        for (var d= 0;d < this.nFileTransfer;d++)if(null == this.fileTransfer[d]){
        this.fileTransfer[d] = new RheaFileDownload (b, a, f, c, e);return
    }this.fileTransfer[this.nFileTransfer++] = new RheaFileDownload (b, a, f, c, e)
    };Rhea.prototype.activateCPUBuzzer = function(b, a, f) {
        var c = new Uint8Array (4);c[0] = RHEA_EVENT_CPU_ACTIVATE_BUZZER;c[1] = parseInt(b);c[2] =
        parseInt(a);c[3] = parseInt(f);this.sendGPUCommand("E", c, 0, 0)
    };
    function Rhea_session_clearValue (b){ store.remove(b) } function Rhea_session_getValue(b) {
        return store.get(
            b
        )
    } function Rhea_session_getOrDefault(b, a) {
        b = Rhea_session_getValue(b);return void 0 === b?a:b
    } function Rhea_session_setValue(b, a) {
        store.set(
            b,
            a
        )
    } function Rhea_session_clearObject(b) { Rhea_session_clearValue(b) } function Rhea_session_setObject(
        b,
        a
    ) {
        Rhea_session_setValue(
            b,
            JSON.stringify(a)
        )
    } function Rhea_session_getObject(b) { return (b = Rhea_session_getValue(b))?JSON.parse(b):b }
    Rhea.prototype.selection_createEmpty =
        function(b) { return { selNum:b, enabled:0, price:"0.00" } };Rhea.prototype.selection_getCount =
        function() { return RHEA_NUM_MAX_SELECTIONS };Rhea.prototype.selection_getBySelNumber =
        function(b) { return 1 > b || b > RHEA_NUM_MAX_SELECTIONS?(rheaLog("ERR:Rhea.selection_getBySelNumber("+b+") => invalid sel number"), this.selection_createEmpty(0)):this.selectionList[b-1] };
    Rhea.prototype.selection_sessionRestore = function() {
        rheaLog("Rhea.selection_sessionRestore()");this.selectionList =
        [];for (var b= 1;b <= RHEA_NUM_MAX_SELECTIONS;b++){
        var a =
            Rhea_session_getObject("selInfo" + b);void 0 === a && (a = this.selection_createEmpty(b), Rhea_session_setObject("selInfo"+b, a));this.selectionList.push(
        a
    )
    }
    };
    Rhea.prototype.selection_sessionStore =
        function() { rheaLog("Rhea.selection_sessionStore()");for (var b= 1;b <= RHEA_NUM_MAX_SELECTIONS;b++)Rhea_session_setObject("selInfo"+b, this.selectionList[b-1]) };Rhea.prototype.selection_start =
        function(b) {
            var a = new Uint8Array (2);a[0] = RHEA_EVENT_START_SELECTION;a[1] =
            parseInt(b);this.sendGPUCommand("E", a, 0, 0)
        };Rhea.prototype.selection_stop = function() {
        var b = new Uint8Array (1);b[0] = RHEA_EVENT_STOP_SELECTION;this.sendGPUCommand(
        "E",
        b,
        0,
        0
    )
    };
    Rhea.prototype.onEvent_selectionAvailabilityUpdated =
        function() {};Rhea.prototype.onEvent_selectionPricesUpdated =
        function() {};Rhea.prototype.onEvent_creditUpdated =
        function() {};Rhea.prototype.onEvent_cpuMessage =
        function(b, a) {};Rhea.prototype.onEvent_selectionReqStatus =
        function(b) {};Rhea.prototype.onEvent_cpuStatus =
        function(b, a, f) {};Rhea.prototype.onEvent_readDataAudit = function(b, a, f) {};
    function RheaFileDownload (b, a, f, c, e){
        this.userValue = a;this.callback_onStart = f;this.callback_onProgress =
        c;this.callback_onEnd = e;this.numOfPacketToBeRcvInTotal = this.fileSize =
        this.smuTransfUID = this.numPacketInAChunk = this.packetSizeInBytes =
        0;this.lastGoodPacket = 4294967295;this.appTransfUID =
        this.priv_generateUID();this.fileBuffer = null;this.fileBufferCT =
        0;this.priv_requestDownload(b)
    }
    RheaFileDownload.prototype.priv_generateUID = function() {
        var b =
            parseInt(1 + Math.floor(254 * Math.random())), a = parseInt(1+Math.floor(254*Math.random())), f = parseInt(1+Math.floor(254*Math.random())), c = parseInt(1+Math.floor(254*Math.random()));return b < <24|a<<16|f<<8|c
    };
    RheaFileDownload.prototype.priv_requestDownload = function(b) {
        var a = new Uint8Array (64), f = 0;a[f++] = 81;a[f++] = b.length;rheaAddU32ToUint8Buffer(
        a,
        f,
        this.appTransfUID
    );f += 4;for (var c= 0;c < b.length;c++)a[f+c] = b.charCodeAt(c);f += b.length;a[f++] =
        0;rhea.sendGPUCommand("F", a.subarray(0, f), 0, 0)
    };
    RheaFileDownload.prototype.priv_on0x52 = function(b) {
        null != this.callback_onStart && "function" === typeof this.callback_onStart && this.callback_onStart(
        this.userValue
    );if (0 != b.reason_refused) return null != this.callback_onEnd && "function" === typeof this.callback_onEnd && this.callback_onEnd(
        this.userValue,
        b.reason_refused,
        null
    ), 0;this.packetSizeInBytes = b.packetSizeInBytes;this.smuTransfUID =
        b.smuTransfUID;this.fileSize = b.fileSize;this.numPacketInAChunk =
        b.numPacketInAChunk;this.lastGoodPacket = 4294967295;this.fileBuffer =
        new Uint8Array (this.fileSize);this.numOfPacketToBeRcvInTotal =
        Math.floor(this.fileSize / this.packetSizeInBytes);this.numOfPacketToBeRcvInTotal * this.packetSizeInBytes < this.fileSize && this.numOfPacketToBeRcvInTotal++;this.priv_sendACK(
        this.lastGoodPacket
    );return 1
    };RheaFileDownload.prototype.priv_sendACK = function(b) {
        var a = new Uint8Array (16), f = 0;a[f++] = 84;rheaAddU32ToUint8Buffer(
        a,
        f,
        this.smuTransfUID
    );f += 4;rheaAddU32ToUint8Buffer(a, f, b);rhea.sendGPUCommand("F", a.subarray(0, f + 4), 0, 0)
    };
    RheaFileDownload.prototype.priv_on0x53 = function(b) {
        var a = this.lastGoodPacket + 1;4294967295 < a && (a = 0);if (b.packetNumReceived == a) {
        this.lastGoodPacket =
            b.packetNumReceived;if (this.lastGoodPacket + 1 >= this.numOfPacketToBeRcvInTotal) return b =
            this.fileSize - this.lastGoodPacket * this.packetSizeInBytes, this.priv_sendACK(this.lastGoodPacket), -b;b.chunkSeq == this.numPacketInAChunk && this.priv_sendACK(
            this.lastGoodPacket
        );return this.packetSizeInBytes
    }this.priv_sendACK(this.lastGoodPacket);return 0
    };
