// ark-pixel-font — shared Ark Pixel font loader with localStorage caching
// Reference from any template that uses the Ark Pixel font.
(function() {
    'use strict';

    var FONT_FAMILY = 'Ark Pixel';
    var FONT_KEY = 'ark-pixel-font-loaded';
    var FONT_TTL = 7 * 24 * 60 * 60 * 1000;

    if (document.fonts && document.fonts.check('1rem "' + FONT_FAMILY + '"')) return;

    var cached = localStorage.getItem(FONT_KEY);
    if (cached && (Date.now() - parseInt(cached, 10)) < FONT_TTL) {
        document.documentElement.classList.add('font-loaded');
        return;
    }

    if (!document.fonts) {
        localStorage.setItem(FONT_KEY, String(Date.now()));
        document.documentElement.classList.add('font-loaded');
        return;
    }

    document.fonts.ready.then(function() {
        if (document.fonts.check('1rem "' + FONT_FAMILY + '"')) {
            localStorage.setItem(FONT_KEY, String(Date.now()));
            document.documentElement.classList.add('font-loaded');
        }
    }).catch(function() {
        localStorage.setItem(FONT_KEY, String(Date.now()));
    });
})();
