document.addEventListener('DOMContentLoaded', function() {
    M.Modal.init(document.getElementById('about'), {});
});

function showCharts ()
{
    document.getElementById('file-upload').style.display = 'none';
    document.getElementById('charts').style.display = 'block';
}
