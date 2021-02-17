var Module=typeof pyodide._module!=="undefined"?pyodide._module:{};Module.checkABI(1);if(!Module.expectedDataFileDownloads){Module.expectedDataFileDownloads=0;Module.finishedDataFileDownloads=0}Module.expectedDataFileDownloads++;(function(){var loadPackage=function(metadata){var PACKAGE_PATH;if(typeof window==="object"){PACKAGE_PATH=window["encodeURIComponent"](window.location.pathname.toString().substring(0,window.location.pathname.toString().lastIndexOf("/"))+"/")}else if(typeof location!=="undefined"){PACKAGE_PATH=encodeURIComponent(location.pathname.toString().substring(0,location.pathname.toString().lastIndexOf("/"))+"/")}else{throw"using preloaded data can only be done on a web page or in a web worker"}var PACKAGE_NAME="more-itertools.data";var REMOTE_PACKAGE_BASE="more-itertools.data";if(typeof Module["locateFilePackage"]==="function"&&!Module["locateFile"]){Module["locateFile"]=Module["locateFilePackage"];err("warning: you defined Module.locateFilePackage, that has been renamed to Module.locateFile (using your locateFilePackage for now)")}var REMOTE_PACKAGE_NAME=Module["locateFile"]?Module["locateFile"](REMOTE_PACKAGE_BASE,""):REMOTE_PACKAGE_BASE;var REMOTE_PACKAGE_SIZE=metadata.remote_package_size;var PACKAGE_UUID=metadata.package_uuid;function fetchRemotePackage(packageName,packageSize,callback,errback){var xhr=new XMLHttpRequest;xhr.open("GET",packageName,true);xhr.responseType="arraybuffer";xhr.onprogress=function(event){var url=packageName;var size=packageSize;if(event.total)size=event.total;if(event.loaded){if(!xhr.addedTotal){xhr.addedTotal=true;if(!Module.dataFileDownloads)Module.dataFileDownloads={};Module.dataFileDownloads[url]={loaded:event.loaded,total:size}}else{Module.dataFileDownloads[url].loaded=event.loaded}var total=0;var loaded=0;var num=0;for(var download in Module.dataFileDownloads){var data=Module.dataFileDownloads[download];total+=data.total;loaded+=data.loaded;num++}total=Math.ceil(total*Module.expectedDataFileDownloads/num);if(Module["setStatus"])Module["setStatus"]("Downloading data... ("+loaded+"/"+total+")")}else if(!Module.dataFileDownloads){if(Module["setStatus"])Module["setStatus"]("Downloading data...")}};xhr.onerror=function(event){throw new Error("NetworkError for: "+packageName)};xhr.onload=function(event){if(xhr.status==200||xhr.status==304||xhr.status==206||xhr.status==0&&xhr.response){var packageData=xhr.response;callback(packageData)}else{throw new Error(xhr.statusText+" : "+xhr.responseURL)}};xhr.send(null)}function handleError(error){console.error("package error:",error)}var fetchedCallback=null;var fetched=Module["getPreloadedPackage"]?Module["getPreloadedPackage"](REMOTE_PACKAGE_NAME,REMOTE_PACKAGE_SIZE):null;if(!fetched)fetchRemotePackage(REMOTE_PACKAGE_NAME,REMOTE_PACKAGE_SIZE,function(data){if(fetchedCallback){fetchedCallback(data);fetchedCallback=null}else{fetched=data}},handleError);function runWithFS(){function assert(check,msg){if(!check)throw msg+(new Error).stack}Module["FS_createPath"]("/","lib",true,true);Module["FS_createPath"]("/lib","python3.8",true,true);Module["FS_createPath"]("/lib/python3.8","site-packages",true,true);Module["FS_createPath"]("/lib/python3.8/site-packages","more_itertools-7.2.0-py3.8.egg-info",true,true);Module["FS_createPath"]("/lib/python3.8/site-packages","more_itertools",true,true);Module["FS_createPath"]("/lib/python3.8/site-packages/more_itertools","tests",true,true);function DataRequest(start,end,audio){this.start=start;this.end=end;this.audio=audio}DataRequest.prototype={requests:{},open:function(mode,name){this.name=name;this.requests[name]=this;Module["addRunDependency"]("fp "+this.name)},send:function(){},onload:function(){var byteArray=this.byteArray.subarray(this.start,this.end);this.finish(byteArray)},finish:function(byteArray){var that=this;Module["FS_createPreloadedFile"](this.name,null,byteArray,true,true,function(){Module["removeRunDependency"]("fp "+that.name)},function(){if(that.audio){Module["removeRunDependency"]("fp "+that.name)}else{err("Preloading file "+that.name+" failed")}},false,true);this.requests[this.name]=null}};function processPackageData(arrayBuffer){Module.finishedDataFileDownloads++;assert(arrayBuffer,"Loading data file failed.");assert(arrayBuffer instanceof ArrayBuffer,"bad input to processPackageData");var byteArray=new Uint8Array(arrayBuffer);var curr;var compressedData={data:null,cachedOffset:124292,cachedIndexes:[-1,-1],cachedChunks:[null,null],offsets:[0,1048,1322,1625,1953,2281,2586,2869,3173,3470,3829,4161,4492,4799,5811,6934,8204,9287,10608,11744,12855,14092,15360,16737,17986,19065,20243,21654,23148,24365,25578,26953,28307,29633,30808,32069,33332,34404,35726,36987,38122,39379,40678,41891,43217,44501,45747,47019,48097,49193,50420,51673,53048,54126,55440,56647,57760,59033,60286,61386,62674,64070,65215,66498,67785,69114,70459,71542,72770,73897,75147,76288,77259,78319,79427,80355,81197,82176,83159,84067,85149,85980,86750,87662,88438,89463,90517,91201,91913,92733,93677,94532,95297,96321,96961,97986,98850,99552,100659,101619,102464,103199,104186,105029,105896,106977,107925,108766,109687,110328,111102,112089,112886,113766,114507,115563,116485,117412,118405,119389,120354,121412,122553,123534],sizes:[1048,274,303,328,328,305,283,304,297,359,332,331,307,1012,1123,1270,1083,1321,1136,1111,1237,1268,1377,1249,1079,1178,1411,1494,1217,1213,1375,1354,1326,1175,1261,1263,1072,1322,1261,1135,1257,1299,1213,1326,1284,1246,1272,1078,1096,1227,1253,1375,1078,1314,1207,1113,1273,1253,1100,1288,1396,1145,1283,1287,1329,1345,1083,1228,1127,1250,1141,971,1060,1108,928,842,979,983,908,1082,831,770,912,776,1025,1054,684,712,820,944,855,765,1024,640,1025,864,702,1107,960,845,735,987,843,867,1081,948,841,921,641,774,987,797,880,741,1056,922,927,993,984,965,1058,1141,981,758],successes:[1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1]};compressedData.data=byteArray;assert(typeof Module.LZ4==="object","LZ4 not present - was your app build with  -s LZ4=1  ?");Module.LZ4.loadPackage({metadata:metadata,compressedData:compressedData});Module["removeRunDependency"]("datafile_more-itertools.data")}Module["addRunDependency"]("datafile_more-itertools.data");if(!Module.preloadResults)Module.preloadResults={};Module.preloadResults[PACKAGE_NAME]={fromCache:false};if(fetched){processPackageData(fetched);fetched=null}else{fetchedCallback=processPackageData}}if(Module["calledRun"]){runWithFS()}else{if(!Module["preRun"])Module["preRun"]=[];Module["preRun"].push(runWithFS)}};loadPackage({files:[{filename:"/lib/python3.8/site-packages/more_itertools-7.2.0-py3.8.egg-info/SOURCES.txt",start:0,end:543,audio:0},{filename:"/lib/python3.8/site-packages/more_itertools-7.2.0-py3.8.egg-info/top_level.txt",start:543,end:558,audio:0},{filename:"/lib/python3.8/site-packages/more_itertools-7.2.0-py3.8.egg-info/PKG-INFO",start:558,end:43407,audio:0},{filename:"/lib/python3.8/site-packages/more_itertools-7.2.0-py3.8.egg-info/dependency_links.txt",start:43407,end:43408,audio:0},{filename:"/lib/python3.8/site-packages/more_itertools/more.py",start:43408,end:126267,audio:0},{filename:"/lib/python3.8/site-packages/more_itertools/recipes.py",start:126267,end:141502,audio:0},{filename:"/lib/python3.8/site-packages/more_itertools/__init__.py",start:141502,end:141589,audio:0},{filename:"/lib/python3.8/site-packages/more_itertools/tests/test_more.py",start:141589,end:233951,audio:0},{filename:"/lib/python3.8/site-packages/more_itertools/tests/test_recipes.py",start:233951,end:253429,audio:0},{filename:"/lib/python3.8/site-packages/more_itertools/tests/__init__.py",start:253429,end:253429,audio:0}],remote_package_size:128388,package_uuid:"1aa6fddd-d3a5-4b78-854c-ab595334b4fc"})})();