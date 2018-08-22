document.addEventListener('DOMContentLoaded', function() {
    M.Modal.init(document.getElementById('about'), {});
});

function showCharts ()
{
    document.getElementById('file-upload').style.display = 'none';
    document.getElementById('charts').style.display = 'block';

    var file = document.getElementById('hist-file').files[0];

    var reader = new FileReader();
    reader.readAsText(file);

    reader.onloadend = (function () {
        var data = reader.result;
        console.log(data);
    });
}
