var Module=typeof pyodide._module!=="undefined"?pyodide._module:{};Module.checkABI(1);if(!Module.expectedDataFileDownloads){Module.expectedDataFileDownloads=0;Module.finishedDataFileDownloads=0}Module.expectedDataFileDownloads++;(function(){var loadPackage=function(metadata){var PACKAGE_PATH;if(typeof window==="object"){PACKAGE_PATH=window["encodeURIComponent"](window.location.pathname.toString().substring(0,window.location.pathname.toString().lastIndexOf("/"))+"/")}else if(typeof location!=="undefined"){PACKAGE_PATH=encodeURIComponent(location.pathname.toString().substring(0,location.pathname.toString().lastIndexOf("/"))+"/")}else{throw"using preloaded data can only be done on a web page or in a web worker"}var PACKAGE_NAME="soupsieve.data";var REMOTE_PACKAGE_BASE="soupsieve.data";if(typeof Module["locateFilePackage"]==="function"&&!Module["locateFile"]){Module["locateFile"]=Module["locateFilePackage"];err("warning: you defined Module.locateFilePackage, that has been renamed to Module.locateFile (using your locateFilePackage for now)")}var REMOTE_PACKAGE_NAME=Module["locateFile"]?Module["locateFile"](REMOTE_PACKAGE_BASE,""):REMOTE_PACKAGE_BASE;var REMOTE_PACKAGE_SIZE=metadata.remote_package_size;var PACKAGE_UUID=metadata.package_uuid;function fetchRemotePackage(packageName,packageSize,callback,errback){var xhr=new XMLHttpRequest;xhr.open("GET",packageName,true);xhr.responseType="arraybuffer";xhr.onprogress=function(event){var url=packageName;var size=packageSize;if(event.total)size=event.total;if(event.loaded){if(!xhr.addedTotal){xhr.addedTotal=true;if(!Module.dataFileDownloads)Module.dataFileDownloads={};Module.dataFileDownloads[url]={loaded:event.loaded,total:size}}else{Module.dataFileDownloads[url].loaded=event.loaded}var total=0;var loaded=0;var num=0;for(var download in Module.dataFileDownloads){var data=Module.dataFileDownloads[download];total+=data.total;loaded+=data.loaded;num++}total=Math.ceil(total*Module.expectedDataFileDownloads/num);if(Module["setStatus"])Module["setStatus"]("Downloading data... ("+loaded+"/"+total+")")}else if(!Module.dataFileDownloads){if(Module["setStatus"])Module["setStatus"]("Downloading data...")}};xhr.onerror=function(event){throw new Error("NetworkError for: "+packageName)};xhr.onload=function(event){if(xhr.status==200||xhr.status==304||xhr.status==206||xhr.status==0&&xhr.response){var packageData=xhr.response;callback(packageData)}else{throw new Error(xhr.statusText+" : "+xhr.responseURL)}};xhr.send(null)}function handleError(error){console.error("package error:",error)}var fetchedCallback=null;var fetched=Module["getPreloadedPackage"]?Module["getPreloadedPackage"](REMOTE_PACKAGE_NAME,REMOTE_PACKAGE_SIZE):null;if(!fetched)fetchRemotePackage(REMOTE_PACKAGE_NAME,REMOTE_PACKAGE_SIZE,function(data){if(fetchedCallback){fetchedCallback(data);fetchedCallback=null}else{fetched=data}},handleError);function runWithFS(){function assert(check,msg){if(!check)throw msg+(new Error).stack}Module["FS_createPath"]("/","lib",true,true);Module["FS_createPath"]("/lib","python3.8",true,true);Module["FS_createPath"]("/lib/python3.8","site-packages",true,true);Module["FS_createPath"]("/lib/python3.8/site-packages","soupsieve-1.9.5-py3.8.egg-info",true,true);Module["FS_createPath"]("/lib/python3.8/site-packages","soupsieve",true,true);function DataRequest(start,end,audio){this.start=start;this.end=end;this.audio=audio}DataRequest.prototype={requests:{},open:function(mode,name){this.name=name;this.requests[name]=this;Module["addRunDependency"]("fp "+this.name)},send:function(){},onload:function(){var byteArray=this.byteArray.subarray(this.start,this.end);this.finish(byteArray)},finish:function(byteArray){var that=this;Module["FS_createPreloadedFile"](this.name,null,byteArray,true,true,function(){Module["removeRunDependency"]("fp "+that.name)},function(){if(that.audio){Module["removeRunDependency"]("fp "+that.name)}else{err("Preloading file "+that.name+" failed")}},false,true);this.requests[this.name]=null}};function processPackageData(arrayBuffer){Module.finishedDataFileDownloads++;assert(arrayBuffer,"Loading data file failed.");assert(arrayBuffer instanceof ArrayBuffer,"bad input to processPackageData");var byteArray=new Uint8Array(arrayBuffer);var curr;var compressedData={data:null,cachedOffset:65891,cachedIndexes:[-1,-1],cachedChunks:[null,null],offsets:[0,926,1732,3013,4471,5530,6761,8013,9205,10224,11227,12034,13274,14250,15371,16290,17479,18321,19333,20415,21553,22655,23621,24292,25056,26049,26938,27966,29060,30116,31088,32127,33123,34222,35260,35947,36980,38095,39396,40661,41610,42822,44103,45221,46571,47737,48710,49592,50685,51693,52770,53385,54283,55190,56232,57185,58157,58947,59707,60880,61962,62833,63783,65183],sizes:[926,806,1281,1458,1059,1231,1252,1192,1019,1003,807,1240,976,1121,919,1189,842,1012,1082,1138,1102,966,671,764,993,889,1028,1094,1056,972,1039,996,1099,1038,687,1033,1115,1301,1265,949,1212,1281,1118,1350,1166,973,882,1093,1008,1077,615,898,907,1042,953,972,790,760,1173,1082,871,950,1400,708],successes:[1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1]};compressedData.data=byteArray;assert(typeof Module.LZ4==="object","LZ4 not present - was your app build with  -s LZ4=1  ?");Module.LZ4.loadPackage({metadata:metadata,compressedData:compressedData});Module["removeRunDependency"]("datafile_soupsieve.data")}Module["addRunDependency"]("datafile_soupsieve.data");if(!Module.preloadResults)Module.preloadResults={};Module.preloadResults[PACKAGE_NAME]={fromCache:false};if(fetched){processPackageData(fetched);fetched=null}else{fetchedCallback=processPackageData}}if(Module["calledRun"]){runWithFS()}else{if(!Module["preRun"])Module["preRun"]=[];Module["preRun"].push(runWithFS)}};loadPackage({files:[{filename:"/lib/python3.8/site-packages/soupsieve-1.9.5-py3.8.egg-info/SOURCES.txt",start:0,end:3805,audio:0},{filename:"/lib/python3.8/site-packages/soupsieve-1.9.5-py3.8.egg-info/top_level.txt",start:3805,end:3815,audio:0},{filename:"/lib/python3.8/site-packages/soupsieve-1.9.5-py3.8.egg-info/PKG-INFO",start:3815,end:9976,audio:0},{filename:"/lib/python3.8/site-packages/soupsieve-1.9.5-py3.8.egg-info/requires.txt",start:9976,end:10031,audio:0},{filename:"/lib/python3.8/site-packages/soupsieve-1.9.5-py3.8.egg-info/dependency_links.txt",start:10031,end:10032,audio:0},{filename:"/lib/python3.8/site-packages/soupsieve/util.py",start:10032,end:14435,audio:0},{filename:"/lib/python3.8/site-packages/soupsieve/css_types.py",start:14435,end:23093,audio:0},{filename:"/lib/python3.8/site-packages/soupsieve/css_match.py",start:23093,end:76508,audio:0},{filename:"/lib/python3.8/site-packages/soupsieve/__meta__.py",start:76508,end:83129,audio:0},{filename:"/lib/python3.8/site-packages/soupsieve/css_parser.py",start:83129,end:126620,audio:0},{filename:"/lib/python3.8/site-packages/soupsieve/__init__.py",start:126620,end:130750,audio:0}],remote_package_size:69987,package_uuid:"aabec015-c3b3-4a16-ac70-b24ebcada66e"})})();