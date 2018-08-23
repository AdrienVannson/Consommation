document.addEventListener('DOMContentLoaded', function() {
    M.Modal.init(document.getElementById('about'), {});
});

var days = [];

function showCharts ()
{
    document.getElementById('file-upload').style.display = 'none';
    document.getElementById('charts').style.display = 'block';
    document.getElementById('arrow-back').style.display = 'block';

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

        createCharts();
    });
}

var chartDailyConsumption = null;
var chartPower = null;

function createCharts ()
{
    // Create the bar chart
    var labels = [];
    var data = [];

    days.forEach(function (day) {
        labels.push(day.data[0].date.split(' ')[0]);
        data.push(day.consumption);
    });

    var chartCanevas = document.getElementById('daily-consumption');
    chartDailyConsumption = new Chart(chartCanevas.getContext('2d'), {
		type: 'bar',
		data: {
			labels: labels,
			datasets: [{
				backgroundColor: "#ffab00", // Amber A400
                borderColor: "#ffab00",
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

    chartCanevas.onclick = function (event) {
        var elements = chartDailyConsumption.getElementsAtEvent(event);

        if (elements.length) {
            var index = elements[0]['_index'];
            showDay(days[index]);
        }
    };

    // Create an empty line chart
    chartPower = new Chart(document.getElementById('power').getContext('2d'), {
        type: 'line',
        data: {
            datasets: []
        },
        options: {
            scales: {
                xAxes: [{
                    type: 'time',
                    display: true,
                    scaleLabel: {
                        display: true,
                        labelString: 'Date'
                    }
                }],
                yAxes: [{
                    ticks: {
                        min: 0
                    }
                }]
            },
            tooltips: {
                mode: 'index',
                intersect: false,
            },
            legend: {
				display: false
			}
        }
	});
}

function showDay (day)
{
    document.getElementById('selected-day').innerText = day.data[0]['date'].split(' ')[0];

    var points = [];
    day.data.forEach(function (info) {
        points.push({
            x: info['date'],
            y: info['consumption']
        });
    });

    chartPower.data.datasets = [{
        data: points,
        borderColor:  "#ffab00", // Amber A400
        fill: false,
        lineTension: 0,
        pointRadius: 0
    }];
    chartPower.update();
}
