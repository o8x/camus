(function() {
    var toggle = document.querySelector('.menu-toggle');
    var sidebar = document.querySelector('.sidebar');
    if (!toggle || !sidebar) return;

    toggle.addEventListener('click', function() {
        sidebar.classList.toggle('open');
    });

    sidebar.addEventListener('click', function(e) {
        if (e.target.tagName === 'A') {
            sidebar.classList.remove('open');
        }
    });
})();
