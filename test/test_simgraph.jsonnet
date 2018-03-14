// These are provided by wire-cell-cfg pacakge which needs to be
// either in WIRECELL_PATH when this file is consumed by wire-cell or
// in a path given by "jsonnet -J <path> ..." if testing with that CLI.
local wc = import "wirecell.jsonnet";
local ar39 = import "ar39.jsonnet";
local v = import "vector.jsonnet";

local cmdline = {
    type: "wire-cell",
    data: {
        plugins: ["WireCellGen", "WireCellPgraph", "WireCellSio"],
        apps: ["Pgrapher"]
    }
};

local random = {
    type: "Random",
    data: {
        generator: "default",
        seeds: [0,1,2,3,4],
    }
};
local utils = [cmdline, random];


// base detector parametrs.  Together, these match no real detector.
// The data structure is organized and named to have synergy with
// configuration objects for the simulation nodes defined later.
local base_params = {
    lar : {
        DL :  7.2 * wc.cm2/wc.s,
        DT : 12.0 * wc.cm2/wc.s,
        lifetime : 8*wc.ms,
        drift_speed : 1.6*wc.mm/wc.us, // 500 V/cm
        density: 1.389*wc.g/wc.centimeter3,
        ar39activity: 1*wc.Bq/wc.kg,
    },
    detector : {
        // Relative extent for active region of LAr box.  
        // (x,y,z) = (drift distance, active height, active width)
        extent: [1*wc.m,1*wc.m,1*wc.m],
        // the center MUST be expressed in the same coordinate system
        // as the wire endpoints given in the files.wires entry below.
        // Default here is that the extent is centered on the origin
        // of the wire coordinate system.
        center: [0,0,0],
        drift_time: self.extent[0]/self.lar.drift_speed,
        drift_volume: v.vmult(self.extent),
        drift_mass: self.lar.density * self.drift_volume,
    },
    daq : {
        readout_time: 5*wc.ms,
        nreadouts: 1,
        start_time: 0.0*wc.s,
        stop_time: self.start_time + self.nreadouts*self.readout_time,
        tick: 0.5*wc.us,        // digitization time period
        sample_period: 0.5*wc.us, // sample time for any spectral data - usually same as tick
        first_frame_number: 100,
        ticks_per_readout: self.readout_time/self.tick,
    },
    adc : {
        gain: 1.0,
        baselines: [900*wc.millivolt,900*wc.millivolt,200*wc.millivolt],
        resolution: 12,
        fullscale: [0*wc.volt, 2.0*wc.volt],
    },
    elec : {
        gain : 14.0*wc.mV/wc.fC,
        shaping : 2.0*wc.us,
        postgain: -1.2,
    },
    sim : {
        fluctuate: true,
        digitize: true,
        noise: false,
    },
    files : {                   // each detector MUST fill these in.
        wires: null,
        fields: null,
        noise: null,
    }
};

local uboone_params = base_params {
    lar : super.lar {
        drift_speed : 1.114*wc.mm/wc.us, // at microboone voltage
    },
    detector : super.detector {
        extent: [2.5604*wc.m,2.325*wc.m,10.368*wc.m],
        // Wires have a detector edge at X=0, Z=0, centered in Y.
        center: [0.5*self.extent[0], 0.0, 0.5*self.extent[2]],
    },
    elec : super.elec {
        postgain: -1.2,
    },
    files : {
        wires:"microboone-celltree-wires-v2.1.json.bz2",
        fields:"ub-10-half.json.bz2",
        noise: "microboone-noise-spectra-v2.json.bz2",
    }
};


local dune_params = base_params {
    lar : super.lar {
        drift_speed : 1.6*wc.mm/wc.us, // 500 V/cm
    },
    detector : super.detector {
        extent : [3.594*wc.m, 5.9*wc.m, 2.2944*wc.m],
        center : [0.5*self.extent[0], 0, 0],
    },  
    elec : super.elec {
        postgain: -1.0,
    },
    files : {
        wires:"dune-wires.json.bz2",
        fields:"garfield-1d-3planes-21wires-6impacts-dune-v1.json.bz2",
        // For now, pretend DUNE is post-noise-filtered microboone for
        // now.  This is bogus in that it gives its spectra for wire
        // lengths which are shorter so extraploation will not be so
        // accurate and may underestimate noise levels.
        noise: "microboone-noise-spectra-v2.json.bz2",
    },
};

local params = uboone_params;


//
// Define some regions and use them for regions in which to generate Ar39 events
//
local bigbox = {
    local half = v.scale(params.detector.extent, 0.5),
    tail: v.topoint(v.vsub(params.detector.center, half)),
    head: v.topoint(v.vadd(params.detector.center, half)),
};
local lilbox = {
    local half = v.frompoint(wc.point(1,1,1,0.5*wc.cm)),
    tail: v.topoint(v.vsub(params.detector.center, half)),
    head: v.topoint(v.vadd(params.detector.center, half)),
};
local ar39blips = { 
    type: "BlipSource",
    name: "fullrate",
    data: {
	charge: ar39,
	time: {
	    type: "decay",
	    start: params.daq.start_time,
            stop: params.daq.stop_time,
	    activity: params.lar.ar39activity * params.detector.drift_mass,
	},
	position: {
	    type:"box",
            extent: bigbox,
	}
    }
};
local debugblips = { 
    type: "BlipSource",
    name: "lowrate",
    data: {
        charge: { type: "mono", value: 10000 },
	time: {
	    type: "decay",
	    start: params.daq.start_time,
            stop: params.daq.stop_time,
            activity: 1.0/(1*wc.ms), // low rate
	},
	position: {
	    type:"box",
	    extent: lilbox,     // localized
	}
    }
};
//local blips = ar39blips;
local blips = debugblips;

//
//  Some basic/bogus cosmic rays
// 
local cosmics = {
    type: "TrackDepos",
    name: "cosmics",
    data: {
        step_size: 1.0 * wc.millimeter,
        tracks: [
            {
                time: 10.0*wc.ns,
                charge: -1,
                ray : wc.ray(wc.point(10,0,0,wc.cm), wc.point(10,0,1,wc.cm))
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
                time: 0.0*wc.ns,
                charge: -1,
                ray : wc.ray(wc.point(10,0,0,wc.cm), wc.point(10,0,1,wc.cm))
            },
            {
                time: 20.0*wc.ns,
                charge: -1,
                ray : wc.ray(wc.point(10,0,0,wc.cm), wc.point(10,0,1,wc.cm))
            },
        ]
    }
};

// Join the depos from the various kinematics.  The DepoMergers only
// do 2-to-1 joining so have to use a few.  They don't take any real
// configuration so just name them here to refer to them later.
local joincb = { type: "DepoMerger", name: "CosmicBeamJoiner" };
local joincbb = { type: "DepoMerger", name: "CBBlipJoiner" };

local kinematics = [blips, cosmics, beam, joincb, joincbb];

// One Anode for each universe to be used in MultiDuctor to
// super-impose different field response functions. 
local anode_nominal = {
    type : "AnodePlane",
    name : "nominal",
    data : params.elec + params.daq + params.files {
        ident : 0,
    }
};
local anode_uvground = anode_nominal {
    name: "uvground",
    data : super.data {
        fields: "ub-10-uv-ground-half.json.bz2",
    }
};
local anode_vyground = anode_nominal {
    name: "vyground",
    data : super.data {
        fields: "ub-10-vy-ground-half.json.bz2",
    }
};
local anodes = [anode_nominal, anode_vyground, anode_uvground];


//
//  Now, the simulation processing component nodes
//

//
// noise simulation parts
//
local noise_model = {
    type: "EmpiricalNoiseModel",
    data: {
        anode: wc.tn(anode_nominal),
        spectra_file: params.files.noise,
        chanstat: "StaticChannelStatus",
        nsamples: params.daq.ticks_per_readout,
    }
};
local noise_source = {
    type: "NoiseSource",
    data: params.daq {
        model: wc.tn(noise_model),
	anode: wc.tn(anode_nominal),
        start_time: params.daq.start_time,
        stop_time: params.daq.stop_time,
        readout_time: params.daq.readout_time,
    }
};
local noise = if params.sim.noise then [noise_model, noise_source] else [];


//
// signal simulation parts
//
local drifter = {
    type : "Drifter",
    data : params.lar + params.sim  {
        anode: wc.tn(anode_nominal),
    }
};

// One ductor for each universe, all identical except for name and the
// coresponding anode.
local ductor_nominal = {
    type : 'Ductor',
    name : 'nominal',
    data : params.daq + params.lar + params.sim {
        nsigma : 3,
	anode: wc.tn(anode_nominal),
    }
};
local ductor_uvground = ductor_nominal {
    name : 'uvground',
    data : super.data {
        anode: wc.tn(anode_uvground),
    }
};
local ductor_vyground = ductor_nominal {
    name : 'vyground',
    data : super.data {
        anode: wc.tn(anode_vyground),
    }
};

// One multiductor to rull them all.
local multi_ductor = {
    type: "MultiDuctor",
    data : {
        anode: wc.tn(anode_nominal),
        chains : [
            [
                {           // select based on transverse location
                    ductor: wc.tn(ductor_uvground),
                    rule: "wirebounds",    // select based on wire bounds.
                    args: [ // If depo is in one of the regions then this ductor is applied.
                        // Each region is specified as a range in u, v and w wire index ranges.
                        // Remember wire index starts counting with 0 at edge/corners at negative-most Z.
                        // Endpoints are considered part of the index range.
                        // Total region is the logical AND of all specified wire index ranges.
                        [
                            { plane: 0, min:100, max:200 },
                            { plane: 1, min:300, max:400 },
                        ],
                        [ // All regions in the list or logically ORed together.
                            { plane: 0, min:500, max:600 }, // just in U
                        ],
                    ],
                },

                {           // select based on transverse location
                    ductor: wc.tn(ductor_vyground),
                    rule: "wirebounds",    // select based on wire bounds.
                    args: [             // If depo is in one of the regions then this ductor is applied.
                        // Each region is specified as a range in u, v and w wire index ranges.
                        // Remember wire index starts counting with 0 at edge/corners at negative-most Z.
                        // Endpoints are considered part of the index range.
                        [ // Total region is the logical AND of all specified wire index ranges.
                            { plane: 1, min:800, max:800 },
                            { plane: 2, min:600, max:700 },
                        ],
                    ],
                },

                {   // if nothing above matches, then use this one
                    ductor: wc.tn(ductor_nominal),
                    rule: "bool",
                    args: true,
                }

            ],
        ],
    }
};

local signal = [drifter, ductor_nominal, ductor_vyground, ductor_uvground, multi_ductor];

// This is used to add noise to signal.  It has not actual
// configuration so just name it.
local frame_summer = {
    type: "FrameSummer",
};

local digitizer = {
    type: "Digitizer",
    data : params.adc {
        anode: wc.tn(anode_nominal),
    }
};

local numpy_saver = {
    data: params.daq {
        //filename: "uboone-wctsim.npz",
        filename: "uboone-wctsim-%(src)s-%(digi)s-%(noise)s.npz" % {
            src: blips.name,
            digi: if params.sim.digitize then "adc" else "volts",
            noise: if params.sim.noise then "noise" else "signal",
        },
        frame_tags: [""],       // untagged.
        scale: if params.sim.digitize then 1.0 else wc.uV,
    }
};
local numpy_depo_saver = numpy_saver { type: "NumpyDepoSaver" };
local numpy_frame_saver = numpy_saver { type: "NumpyFrameSaver" };


// not configurable, just name it.
local frame_sink = { type: "DumpFrames" };

local readout = [digitizer, numpy_depo_saver, numpy_frame_saver];

// Here the nodes are joined into a graph for execution by the main
// app object.  
local app = {
    type: "Pgrapher",
    data: {
        edges: [
            {
                tail: { node: wc.tn(cosmics) },
                head: { node: wc.tn(joincb), port:0 }
            },
            {
                tail: { node: wc.tn(beam) },
                head: { node: wc.tn(joincb), port:1 }
            },
            {
                tail: { node: wc.tn(joincb) },
                head: { node: wc.tn(joincbb), port:0 }
            },
            {
                tail: { node: wc.tn(blips) },
                head: { node: wc.tn(joincbb), port:1 }
            },
            {
                tail: { node: wc.tn(joincbb) },
                head: { node: wc.tn(numpy_depo_saver) },
            },
            {
                tail: { node: wc.tn(numpy_depo_saver) },
                head: { node: wc.tn(drifter) },
            },

            {
                tail: { node: wc.tn(drifter) },
                head: { node: wc.tn(multi_ductor) },
            }
        ] + ( if params.sim.noise then [
            // noise
            {
                tail: { node: wc.tn(multi_ductor) },
                head: { node: wc.tn(frame_summer), port:0 },
            },
            {
                tail: { node: wc.tn(noise_source) },
                head: { node: wc.tn(frame_summer), port:1 },
            },

            {
                tail: { node: wc.tn(frame_summer) },
                head: { node: wc.tn(digitizer) },
            }
        ] else [
            // no noise
            {
                tail: { node: wc.tn(multi_ductor) },
                head: { node: wc.tn(digitizer) },
            }
        ]) + [
            {
                tail: { node: wc.tn(digitizer) },
                head: { node: wc.tn(numpy_frame_saver) },
            },

            
            {                   // terminate the stream
                tail: { node: wc.tn(numpy_frame_saver) },
                head: { node: wc.tn(frame_sink) },
            },
        ]
    }
};

// Finally, we return the actual configuration sequence:
utils + kinematics + anodes + noise + signal + readout + [app]

