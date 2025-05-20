function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function heat(pb_status) {
    if (pb_status == true) {
        $.ajax({url: "/switch?action=poweron",
                success: function (data) {
                    updateGui(data);
                }
            });
    } else {
        $.ajax({url: "/switch?action=poweroff",
                success: function (data) {
                    updateGui(data);
                }
            });
    }
}

function air(pv_status) {
    switch (pv_status) {
        case true:
            $.ajax({
                url: "/switch?action=airon",
                success: function (data) {
                    updateGui(data);
                }
            });
            break;
        case false:
            $.ajax({
                url: "/switch?action=airoff",
                success: function (data) {
                    updateGui(data);
                }
            });
            break;
        case "shot":
            $.ajax({
                url: "/switch?action=shot",
                success: function (data) {
                    updateGui(data);
                }
            });
            break;
        case "little":
            $.ajax({
                url: "/switch?action=little",
                success: function (data) {
                    updateGui(data);
                }
            });
            break;
        case "big":
            $.ajax({
                url: "/switch?action=big",
                success: function (data) {
                    updateGui(data);
                }
            });
            break;
        case "stop":
            $.ajax({
                url: "/switch?action=stop",
                success: function (data) {
                    updateGui(data);
                }
            });
            break;
    }
}

function updateTimer() {
    num = window.data.poweroff;
    if (num > 0) {
        $("#fill").show();
        $('.loading span').html(num + 's');
        window.data.poweroff = num - 1;
    } else {
        update();
    }
}

function updateGui(data) {
    response = data;
    if (response.heat == 1) {
        $("#heaton").hide();
        $("#heatoff").show();
    } else {
        $("#heaton").show();
        $("#heatoff").hide();
    }

    num = Math.ceil(response.poweroff / 1000);
    window.data = {};
    window.data.poweroff = num;
    if (updateInterval != null) {
        clearInterval(updateInterval);
    }

    if (response.air == 1 && response.poweroff > 0) {
        if (num > 0) {
            updateTimer();
            updateInterval = setInterval(updateTimer, 1000);
        }
        $("#shot").hide();
        $("#little").hide();
        $("#big").hide();
        $("#stop").show();
        $("#heatoff").hide();
        $("#heaton").hide();
    } else {
        $("#shot").show();
        $("#little").show();
        $("#big").show();
        $("#fill").hide();
        $("#stop").hide();
    }
}

function update() {
    $.ajax({
        url: "/status", success: function (result) {
            updateGui($.parseJSON(result));
        },
        error: function (result, error) {
            console.log(error);
        },
        dataType: 'text'
    });
}

updateInterval = null;
update();
setInterval(update, 10000);