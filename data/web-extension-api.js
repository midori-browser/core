// Promise-based message handler
var promises = [];
var last_promise = 0;
var m = function (fn, args, cb) {
    var promise = new Promise (function (resolve, reject) {
        window.webkit.messageHandlers.midori.postMessage ({fn: fn, args: args, promise: last_promise});
        last_promise = promises.push({resolve: resolve, reject: reject});
    });
    return promise;
}

// Browser API
window.browser = {
    tabs: {
        create: function (args, cb) { return m ('tabs.create', args, cb); },
        executeScript: function (args, cb) { return m ('tabs.executeScript', args, cb); },
    },
    notifications: {
        create: function (args, cb) { return m ('notifications.create', args, cb); },
    }
}

// Compatibility with Chrome
window.chrome = window.browser;
