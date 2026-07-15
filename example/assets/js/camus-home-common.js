(function() {
    var formatDate = function(ts) {
        var n = Number(ts);
        if (!n || isNaN(n)) return '';
        var d = new Date(n * 1000);
        if (isNaN(d.getTime())) return '';
        return d.getFullYear() + '-' +
            String(d.getMonth() + 1).padStart(2, '0') + '-' +
            String(d.getDate()).padStart(2, '0');
    };

    var normalizePath = function(p) {
        if (!p || p === '/') return '/';
        if (!p.startsWith('/')) p = '/' + p;
        if (p.length > 1 && p.endsWith('/')) p = p.slice(0, -1);
        return p;
    };

    var encodePath = function(s) {
        return /[\u4e00-\u9fff]/.test(s) ? encodeURIComponent(s) : s;
    };

    var getParent = function(p) {
        var np = normalizePath(p);
        if (np === '/') return null;
        var parts = np.split('/');
        parts.pop();
        if (parts.length === 0 || (parts.length === 1 && parts[0] === '')) return '/';
        return parts.join('/');
    };

    var getDirParam = function() {
        var m = window.location.search.match(/[?&]dir=([^&]*)/);
        return m ? decodeURIComponent(m[1]) : '';
    };

    var findSiteEntry = function(flatData) {
        for (var i = 0; i < flatData.length; i++) {
            if (flatData[i].is_dir && normalizePath(flatData[i].path) === '/') {
                return flatData[i];
            }
        }
        return { path: '/', title: '', subtitle: '', desc: '' };
    };

    var buildTocData = function(flatData, encodeFn) {
        var dirParam = getDirParam();
        var currentPath = normalizePath(dirParam || '/');

        var currentEntry = null;
        for (var i = 0; i < flatData.length; i++) {
            if (flatData[i].is_dir && normalizePath(flatData[i].path) === currentPath) {
                currentEntry = flatData[i];
                break;
            }
        }
        if (!currentEntry) currentPath = '/';

        var items = [];

        if (dirParam && currentPath !== '/') {
            var parent = getParent(currentPath);
            var backLabel;
            var backUrl;
            if (parent) {
                if (parent === '/') {
                    backLabel = '/';
                    backUrl = '/';
                } else {
                    backLabel = '../';
                    backUrl = '/?dir=' + encodeFn(parent.replace(/^\//, ''));
                }
            } else {
                backLabel = '/';
                backUrl = '/';
            }
            items.push({ type: 'back', label: backLabel, url: backUrl, time: '' });
        }

        var subDirs = [];
        var files = [];
        var pfx = currentPath === '/' ? '/' : currentPath + '/';

        var hasDirSet = {};
        for (var d = 0; d < flatData.length; d++) {
            if (flatData[d].is_dir) hasDirSet[normalizePath(flatData[d].path)] = true;
        }

        for (var j = 0; j < flatData.length; j++) {
            var entry = flatData[j];
            var ep = normalizePath(entry.path);
            if (ep === currentPath) continue;
            if (!ep.startsWith(pfx)) continue;
            var remainder = ep.slice(pfx.length);
            if (remainder.indexOf('/') !== -1) {
                var parts = remainder.split('/');
                var skip = false;
                var dirPrefix = currentPath === '/' ? '/' : currentPath + '/';
                for (var p = 0; p < parts.length - 1; p++) {
                    if (hasDirSet[dirPrefix + parts.slice(0, p + 1).join('/')]) { skip = true; break; }
                }
                if (skip) continue;
            }
            if (entry.is_dir) {
                subDirs.push(entry);
            } else {
                var link = entry.link_path || ep;
                files.push({ label: entry.title, url: normalizePath(link), time: formatDate(entry.write_time) });
            }
        }

        for (var k = 0; k < subDirs.length; k++) {
            var cdp = normalizePath(subDirs[k].path);
            var encodedPath = cdp.replace(/^\//, '');
            items.push({
                type: 'dir',
                label: subDirs[k].title,
                url: '/?dir=' + encodeFn(encodedPath),
                time: formatDate(subDirs[k].write_time)
            });
        }

        for (var f = 0; f < files.length; f++) {
            items.push({
                type: 'file',
                label: files[f].label,
                url: files[f].url,
                time: files[f].time
            });
        }

        return { items: items, currentEntry: currentEntry, currentPath: currentPath };
    };

    var updateTocMeta = function(currentEntry, currentPath, siteEntry) {
        var descEl = document.getElementById('cover-description');
        if (descEl && currentEntry && currentEntry.desc) {
            descEl.textContent = currentEntry.desc;
            descEl.style.display = '';
        } else if (descEl) {
            if (!currentEntry || normalizePath(currentEntry.path) === '/') {
                descEl.textContent = siteEntry.desc || '';
                descEl.style.display = '';
            } else {
                descEl.style.display = 'none';
            }
        }

        var titleEl = document.getElementById('page-title');
        if (titleEl && currentEntry && currentEntry.title && currentPath !== '/') {
            titleEl.textContent = currentEntry.title + " - " + siteEntry.title;
        } else if (titleEl) {
            titleEl.textContent = siteEntry.title + " - " + siteEntry.subtitle;
        }

        var covTitleEl = document.getElementById('cover-title');
        if (covTitleEl && currentEntry && currentEntry.title && currentPath !== '/') {
            covTitleEl.textContent = currentEntry.title;
        } else if (covTitleEl) {
            covTitleEl.textContent = siteEntry.title;
        }

        var covSubEl = document.querySelector('.cover-subtitle');
        if (covSubEl && currentEntry && currentEntry.subtitle && currentPath !== '/') {
            covSubEl.textContent = currentEntry.subtitle;
        } else if (covSubEl) {
            covSubEl.textContent = siteEntry.subtitle;
        }
    };

    var renderList = function(items, animateCount, animStep) {
        var fileIndex = 0;
        return items.map(function(item, i) {
            var extra = '';
            if (animateCount > 0 && i < animateCount) {
                extra = ' style="animation-delay:' + (i * animStep).toFixed(2) + 's"';
            }
            var extraClass = '';
            if (item.type === 'dir') {
                extraClass = ' toc-dir';
            } else if (item.type === 'back') {
                extraClass = ' toc-back-root';
            }
            var num = '';
            if (item.type === 'file') {
                fileIndex++;
                num = '<span class="toc-num">' + String(fileIndex).padStart(3, '0') + '</span>';
            } else if (item.type === 'dir') {
                num = '<span class="toc-num">[+]</span>';
            } else {
                num = '<span class="toc-num">[/]</span>';
            }
            return '<li class="toc-item' + extraClass + '"' + extra + '>' +
                num +
                '<a class="toc-link" href="' + item.url + '">' + item.label + '</a>' +
                '<span class="toc-date">' + item.time + '</span>' +
                '</li>';
        }).join('');
    };

    var init = function(options) {
        var opts = options || {};
        var encodeFn = opts.encodeFn || encodeURIComponent;
        var animateCount = opts.animateCount || 0;
        var animStep = opts.animStep || 0.03;

        fetch('/toc.json')
            .then(function(res) { return res.json(); })
            .then(function(flatData) {
                var siteEntry = findSiteEntry(flatData);
                var result = buildTocData(flatData, encodeFn);
                updateTocMeta(result.currentEntry, result.currentPath, siteEntry);

                document.getElementById('toc-list').innerHTML = renderList(result.items, animateCount, animStep);

                if (opts.onRender) {
                    opts.onRender(result.items);
                }
            })
            .catch(function(err) { console.error('Failed to load toc.json:', err); });
    };

    window.HomeCommon = {
        formatDate: formatDate,
        normalizePath: normalizePath,
        encodePath: encodePath,
        getParent: getParent,
        getDirParam: getDirParam,
        buildTocData: buildTocData,
        updateTocMeta: updateTocMeta,
        renderList: renderList,
        init: init
    };
})();
