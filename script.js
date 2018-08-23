document.addEventListener('DOMContentLoaded', function() {
    M.Modal.init(document.getElementById('about'), {});
});

var days = [];

function showCharts ()
{
    document.getElementById('file-upload').style.display = 'none';
    document.getElementById('charts').style.display = 'block';

    var file = document.getElementById('hist-file').files[0];

    var reader = new FileReader();
    reader.readAsText(file);

    reader.onloadend = (function () {
        var data = reader.result;

        data = data.split('\n');
        data.splice(0, 1);
        data = data.join('\n');

        var lastConsumption = null;
        var lastDate = data.split(' ')[0];

        var day = [];
        var dailyConsumption = 0;

        data.split('\n').forEach(function(line) {
            var date = line.split(' ')[0];

            var totalConsumption = parseInt(line.split(' ')[2]) + parseInt(line.split(' ')[3]);
            var consumption = totalConsumption - lastConsumption;

            if (date != lastDate) {
                days.push({
                    'consumption': dailyConsumption,
                    'data': day
                });

                lastDate = date;
                day = [];
                dailyConsumption = 0;
            }

            if (isNaN(consumption)) {
                return;
            }

            if (lastConsumption !== null) {

                dailyConsumption += consumption;

                day.push({
                    'date': line.substring(0, 16),
                    'consumption': consumption
                });
            }

            lastConsumption = totalConsumption;
        });

        createBarChart();
    });
}

function createBarChart ()
{
    var labels = [];
    var data = [];

    days.forEach(function (day) {
        labels.push(day.data[0].date.split(' ')[0]);
        data.push(day.consumption);
    });

    new Chart(document.getElementById('daily-consumption').getContext('2d'), {
		type: 'bar',
		data: {
			labels: labels,
			datasets: [{
				backgroundColor: "rgba(54, 162, 235, 0.7)",
				borderColor: "rgba(54, 162, 235, 1)",
				borderWidth: 1,
				data: data
			}]
		},
		options: {
			responsive: true,
			legend: {
				display: false
			},
            scales: {
                yAxes: [{
                    ticks: {
                        min: 0
                    }
                }]
            }
		}
	});
}
