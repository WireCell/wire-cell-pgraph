local wc = import "wirecell.jsonnet";

local cmdline = {
  type: "wire-cell",
  data: {
    plugins: ["WireCellGen", "WireCellPgraph"],
    apps: ["Pgrapher"]
  }
};


local cosmics = {
  type: "TrackDepos",
  name: "cosmics",
  data: {
    step_size: 1.0 * wc.millimeter,
    tracks: [
      {
        time: 10.0*wc.ns,
        charge: -1,
        ray : wc.ray(wc.point(10,0,0,wc.cm), wc.point(100,10,10,wc.cm))
      },
    ]
  }
};
local beam = {
  type: "TrackDepos",
  name: "beam",
  data: {
    step_size: 1.0 * wc.millimeter,
    tracks: [
      {
        time: 20.0*wc.ns,
        charge: -1,
        ray : wc.ray(wc.point(20,0,0,wc.cm), wc.point(100,20,20,wc.cm))
      },
    ]
  }
};
local depojoin = {
  type: "DepoMerger",
};

local app = {
  type: "Pgrapher",
  data: {
    edges: [
      {
        tail: { node: wc.tn(cosmics) },
        head: { node: wc.tn(depojoin), port:0 }
      },
      {
        tail: { node: wc.tn(beam) },
        head: { node: wc.tn(depojoin), port:1 }
      },
      {
        tail: { node: wc.tn(depojoin) },
        head: { node: "DumpDepos" }
      },
    ]
  }
};

[ cmdline, cosmics, beam, depojoin, app ]
