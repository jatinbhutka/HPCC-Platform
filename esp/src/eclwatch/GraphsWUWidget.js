define([
    "dojo/_base/declare",

    "dijit/layout/ContentPane",

    "dgrid/selector",

    "hpcc/GraphsWidget",
    "src/ESPWorkunit",
    "hpcc/DelayLoadWidget",

    "@hpcc-js/eclwatch"

], function (declare,
    ContentPane,
    selector,
    GraphsWidget, ESPWorkunit, DelayLoadWidget,
    hpccEclWatch) {
        return declare("GraphsWUWidget", [GraphsWidget], {
            wu: null,

            postCreate: function (args) {
                this.inherited(arguments);
                this.timelinePane = new ContentPane({
                    id: this.id + "TimelinePane",
                    region: "top",
                    splitter: true,
                    style: "height: 120px",
                    minSize: 120
                });
                this.timelinePane.placeAt(this.gridTab, "last");
                var context = this;
                var origResize = this.timelinePane.resize;
                this.timelinePane.resize = function () {
                    origResize.apply(this, arguments);
                    if (context.timeline) {
                        context.timeline
                            .resize()
                            .lazyRender()
                            ;
                    }
                }
            },

            init: function (params) {
                if (this.inherited(arguments))
                    return;

                if (params.Wuid) {
                    var context = this;
                    this.wu = ESPWorkunit.Get(params.Wuid);
                    var monitorCount = 4;
                    this.wu.monitor(function () {
                        if (context.wu.isComplete() || ++monitorCount % 5 === 0) {
                            context.refreshGrid();
                        }
                    });

                    this.timeline = new hpccEclWatch.WUTimeline()
                        .target(this.id + "TimelinePane")
                        .overlapTolerence(1)
                        .baseUrl("")
                        .wuid(params.Wuid)
                        .on("dblclick", function (row, col, sel) {
                            if (row && row.__lparam && event && event.ctrlKey) {
                                var scope = row.__lparam;
                                switch (scope.ScopeType) {
                                    case "graph":
                                        var tab = context.ensurePane({ Name: row.label });
                                        context.selectChild(tab);
                                        break;
                                    default:
                                        var descendents = scope.ScopeName.split(":");
                                        for (var i = 0; i < descendents.length; ++i) {
                                            var scopeName = descendents[i];
                                            if (scopeName.indexOf("graph") === 0) {
                                                var tab = context.ensurePane({ Name: scopeName }, { SubGraphId: row.label });
                                                context.selectChild(tab);
                                                break;
                                            }
                                        }
                                }
                            }
                        }, true)
                        .render()
                        ;
                }

                this._refreshActionState();
            },

            createGridColumns: function () {
                var context = this;
                return {
                    col1: selector({
                        width: 27,
                        selectorType: 'checkbox'
                    }),
                    Name: {
                        label: this.i18n.Name, width: 99, sortable: true,
                        formatter: function (Name, row) {
                            return context.getStateImageHTML(row) + "&nbsp;<a href='#' class='dgrid-row-url'>" + Name + "</a>";
                        }
                    },
                    Label: { label: this.i18n.Label, sortable: true },
                    WhenStarted: {
                        label: this.i18n.Started, width: 90,
                        formatter: function (whenStarted) {
                            if (whenStarted) {
                                var dateTime = new Date(whenStarted);
                                return dateTime.toLocaleTimeString();
                            }
                            return "";
                        }
                    },
                    WhenFinished: {
                        label: this.i18n.Finished, width: 90,
                        formatter: function (whenFinished, idx) {
                            if (whenFinished) {
                                var dateTime = new Date(whenFinished);
                                return dateTime.toLocaleTimeString();
                            }
                            return "";
                        }
                    },
                    Time: {
                        label: this.i18n.Duration, width: 90, sortable: true,
                        formatter: function (totalSeconds, idx) {
                            var hours = Math.floor(totalSeconds / 3600);
                            totalSeconds %= 3600;
                            var minutes = Math.floor(totalSeconds / 60);
                            var seconds = (totalSeconds % 60).toFixed(2);
                            return (hours < 10 ? "0" : "") + hours + ":" + (minutes < 10 ? "0" : "") + minutes + ":" + (seconds < 10 ? "0" : "") + seconds;
                        }
                    },
                    Type: { label: this.i18n.Type, width: 72, sortable: true }
                };
            },

            localParams: function (_id, row, params) {
                return {
                    Wuid: this.wu.Wuid,
                    GraphName: row.Name,
                    SubGraphId: (params && params.SubGraphId) ? params.SubGraphId : null,
                    SafeMode: (params && params.safeMode) ? true : false
                };
            },

            refreshGrid: function (args) {
                var context = this;
                this.wu.getInfo({
                    onGetTimers: function (timers) {
                        //  Required to calculate Graphs Total Time  ---
                    },
                    onGetGraphs: function (graphs) {
                        context.store.setData(graphs);
                        context.grid.refresh();
                    }
                });
            }
        });
    });
