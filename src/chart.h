#include <Arduino.h>

#ifndef chart_h
#define chart_h

const char HTTP_CHART_HEADER[] PROGMEM = "<script src = 'https://code.highcharts.com/highcharts.js'></script>"
                                         "<style>"
                                         ".container {min-width:260px;"
                                         "max-width:500px;"
                                         "height:250px;"
                                         "}"
                                         "</style>";
const char HTTP_CHART_DIV[] PROGMEM = " <h2>{title}</h2><div id='{id}' class='container'></div>";
const char HTTP_CHART_RENDER[] PROGMEM =
    "<script> var chart{id} = new Highcharts.Chart({"
    "    chart : {renderTo : '{id}'},"
    "    title : {text : '{title}'},"
    "    series : [ {"
    "        showInLegend : false,"
    "        data : []"
    "    } ],"
    "plotOptions : {"
    "line : {animation : false,"
    "dataLabels:"
    "{"
    "enabled : true"
    "}"
    "}"
    ","
    "series:"
    "{"
    "color : '#059e8a'"
    "}"
    "},"
    "xAxis : {type : 'datetime',"
    "dateTimeLabelFormats:"
    "{"
    "second : '%k:%S'"
    "}"
    "}"
    ","
    "yAxis : {"
    "title:"
    "{"
    "text : '{xtitle}'"
    "}"
    "},"
    "credits:"
    "{"
    "enabled : false"
    "}"
    "});"
    "setInterval("
    "function()"
    "{"
    "var xhttp = new XMLHttpRequest();"
    "xhttp.onreadystatechange = function()"
    "{"
    "if (this.readyState == 4 && this.status == 200)"
    "{"
    "    var x = (new Date()).getTime(),"
    "y = parseFloat(this.responseText);"
    "console.log(this.responseText,'(',x,y,')');"
    "if (chart{id}.series[0].data.length > 40)"
    "{"
    "chart{id}.series[0]"
    "    .addPoint([ x, y ], true, true, true);"
    "}"
    "else"
    "{"
    "chart{id}.series[0]"
    "    .addPoint([ x, y ], true, false, true);"
    "}"
    "}"
    "};"
    "xhttp.open('GET', '{link}', true);"
    "xhttp.send();"
    "},"
    "30000);</script>";

class Chart
{
public:
    String render(String id, String title, String xtitle, String link)
    {
        String s = div(id, "");
        s += chartObject(id, title, xtitle, link);
        return s;
    }

private:
    String chartObject(String id, String title, String xtitle, String link)
    {
        String s = FPSTR(HTTP_CHART_RENDER);
        s.replace("{link}", link);
        s.replace("{id}", id);
        s.replace("{title}", title);
        s.replace("{xtitle}", xtitle);
        return s;
    }
    String div(String id, String title)
    {
        String s = FPSTR(HTTP_CHART_DIV);
        s.replace("{id}", id);
        s.replace("{title}", title);
        return s;
    }
};

#endif