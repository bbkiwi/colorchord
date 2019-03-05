//Copyright (C) 2015 <>< Charles Lohr, see LICENSE file for more info.
//
//This particular file may be licensed under the MIT/x11, New BSD or ColorChord Licenses.
var wsUri = "ws://" + location.host + "/d/ws/issue";
var output;
var websocket;
var commsup = 0;

var mpfs_start_at = {{PAGE_OFFSET}}; //65536; //1048576; NOTE: If you select 1048576, it will override the 65536 sector, but has much more room.
var flash_scratchpad_at = 524288;
var flash_blocksize = 65536;
var flash_sendsize = 256;
//Push objects that have:
// .request
// .callback = function( ref (this object), data );

var workqueue = [];
var wifilines = [];
var workarray = {};
var lastitem;

var SystemMessageTimeout = null;
function IssueSystemMessage( msg )
{
	var elem = $( "#SystemMessage" );
	elem.hide();
	elem.html(  "<font size=+2>" + msg + "</font>" );
	elem.slideToggle( 'fast' );
	if( SystemMessageTimeout != null ) clearTimeout(SystemMessageTimeout);
	SystemMessageTimeout = setTimeout( function() { SystemMessageTimeout = null; $( "#SystemMessage" ).fadeOut( 'slow' ) }, 3000 );
}

function QueueOperation( command, callback )
{
	if( workarray[command] == 1 )
	{
		return;
	}

	workarray[command] = 1;
	var vp = new Object();
	vp.callback = callback;
	vp.request = command;
	workqueue.push( vp );
}

did_init = false;
function init()
{
	if( did_init ) return;
	did_init = true;

	$('#MainMenu > tbody:first-child').before( "\
		<tr><td width=1> \
		<input type=submit onclick=\"ShowHideEvent( 'SystemStatus' ); SystemInfoTick();\" value='System Status' id=SystemStatusClicker></td><td> \
		<div id=SystemStatus class='collapsible'> \
		<table width=100% border=1><tr><td> \
<div id=output></div><div id=systemsettings></div></td></tr></table></div></td></tr>"
	);

	$( ".collapsible" ).each(function( index ) {
		if( localStorage["sh" + this.id] > 0.5 )
		{
			$( this ).show().toggleClass( 'opened' );
			console.log( "OPEN: " + this.id );
		}
	});

	$("#custom_command_response").val( "" );

	output = document.getElementById("output");

	console.log( "Load complete.\n" );
	Ticker();
}

window.addEventListener("load", init, false);


function StartWebSocket()
{
	output.innerHTML = "Connecting...";
	if( websocket ) websocket.close();
	workarray = {};
	workqueue = [];
	lastitem = null;
	websocket = new WebSocket(wsUri);
	websocket.binaryType = 'arraybuffer';
	websocket.onopen = function(evt) { onOpen(evt) };
	websocket.onclose = function(evt) { onClose(evt) };
	websocket.onmessage = function(evt) { startDelayedonMessage(evt) };
	websocket.onerror = function(evt) { onError(evt) };
}

function onOpen(evt)
{
	doSend('e' );
}

function onClose(evt)
{
	$('#SystemStatusClicker').css("color", "red" );
	commsup = 0;
}

var msg = 0;
var tickmessage = 0;
var lasthz = 0;
var time_since_hz = 10; //Make it realize it was disconnected to begin with.

function Ticker()
{
	setTimeout( Ticker, 1000 );

	lasthz = (msg - tickmessage);
	tickmessage = msg;
	if( lasthz == 0 )
	{
		time_since_hz++;
		if( time_since_hz > 3 )
		{
			$('#SystemStatusClicker').css("color", "red" );
			$('#SystemStatusClicker').prop( "value", "System Offline" );
			if( commsup != 0 ) IssueSystemMessage( "Comms Lost." );
			commsup = 0;
			StartWebSocket();
		}
		else
			$('#SystemStatusClicker').prop( "value", "System " + 0 + "Hz" );
	}
	else
	{
		time_since_hz = 0;
		$('#SystemStatusClicker').prop( "value", "System " + lasthz + "Hz" );
	}
}

function startDelayedonMessage(evt)
{
  setTimeout(onMessage, 10, evt) //wait  before calling onMessage
}

function onMessage(evt)
{
	//This delay slows down the rate of all message responses to 50 Hz
	//which seems to not cause much interferance
	//Too fast rate was causing interferance with ADC.
	//var i;
	//for (i=0; i<0xffffff;i++) {
	//}

	msg++;


	if( commsup != 1 )
	{
		commsup = 1;
		$('#SystemStatusClicker').css("color", "green" );
		IssueSystemMessage( "Comms Established." );
	}


	var rawdat = new Uint8Array(evt.data)
	var stringdata = String.fromCharCode.apply(null, rawdat);

	if( lastitem )
	{
		if( lastitem.callback )
		{
			lastitem.callback( lastitem, stringdata, rawdat );
			lastitem = null;
		}
	}
	else
	{
		if( stringdata.length > 2 )
		{
			var wxresp = stringdata.substr(2).split("\t");
			output.innerHTML = "Messages: " + msg + "  RSSI: " + wxresp[0] + " / IP: " + ((wxresp.length>1)?HexToIP( wxresp[1] ):"") + "";
		}
	}


	if( workqueue.length )
	{
		var elem = workqueue.shift();
		delete workarray[elem.request];

		if( elem.request )
		{
			doSend( elem.request );
			lastitem = elem;
			return;
		}
	}

// putting long delay loop here ONLY slows down when all other gui windows
// close. Get Hz of 1 or 2 and very few glitches, but gui takes long time to
// respond to push of button
//	var i;
//	for (i=0; i<0xffffffff;i++) {
//	}

	doSend('wx'); //Request RSSI.
} //end onMessage()

function onError(evt)
{
	$('#SystemStatusClicker').css("color", "red" );
	commsup = 0;
}

function doSend(message)
{
	websocket.send(message);
}

function IsTabOpen( objname )
{
	var obj = $( "#" + objname );
	var opened = obj.is( '.opened' );
	return opened != 0;
}

function ShowHideEvent( objname )
{
	var obj = $( "#" + objname );
	obj.slideToggle( 'fast' ).toggleClass( 'opened' );
	var opened = obj.is( '.opened' );
	localStorage["sh" + objname] = opened?1:0;
	return opened!=0;
}


function IssueCustomCommand()
{
	QueueOperation( $("#custom_command").val(), function( req,data) { $("#custom_command_response").val( data ); } );
}











///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///Below here are mostly just events...

var sysset = null;
var snchanged = false;
var sdchanged = false;

var lastpeerdata = "";

function CallbackForPeers(req,data)
{
	if( data == lastpeerdata ) return;
	lastpeerdata = data;
	var lines = data.split( "\n" );
	var searchcount = 0;
	if( lines.length > 0 )
	{
		var line1 = lines[0].split( "\t" );
		if( line1.length > 1 ) searchcount = Number( line1[1] );
	}

	var htm = "<TABLE BORDER=1 STYLE='width:150'><TR><TH>Address</TH><TH>Service</TH><TH>Name</TH><TH>Description</TH></TR>";
	for( var i = 1; i < lines.length; i++ )
	{
		var elems = lines[i].split( "\t" );
		if( elems.length < 4 ) continue;
		IP = HexToIP( elems[0] );

		htm += "<TR><TD><A HREF=http://" + IP + ">" + IP + "</A></TD><TD>" + elems[1] + "</TD><TD>" + elems[2] + "</TD><TD>" + elems[3] + "</TD></TR>";
	}
	htm += "</TABLE>";
	if( searchcount == 0 )
	{
		htm += "<INPUT TYPE=SUBMIT VALUE=\"Initiate Search\" ONCLICK='QueueOperation(\"BS\");'>";
	}

	$("#peers").html( htm );
}

function SysTickBack(req,data)
{
	var params = data.split( "\t" );
	if( !snchanged )
	{
		$("#SystemName").prop( "value", params[3] );
		$("#SystemName").removeClass( "unsaved-input");
	}
	if( !sdchanged )
	{
		$("#SystemDescription").prop( "value", params[4] );
		$("#SystemDescription").removeClass( "unsaved-input");
	}
	$("#ServiceName").html( params[5] );
	$("#FreeHeap").html( params[6] );

	QueueOperation( "BL", CallbackForPeers );
}

function SystemInfoTick()
{
	if( IsTabOpen('SystemStatus') )
	{
		QueueOperation( "I", SysTickBack );
		setTimeout( SystemInfoTick, 500 );
	}
	else
	{
		//Stop.
	}
}

function SystemChangesReset()
{
	snchanged = false;
	sdchanged = false;
}

function SystemUncommittedChanges()
{
	if( sdchanged || snchanged ) return true;
	else return false;
}


function tohex8( c )
{
	var hex = c.toString(16);
	return hex.length == 1 ? "0" + hex : hex;
}


function HexToIP( hexstr )
{
	if( !hexstr ) return "";
	return parseInt( hexstr.substr( 6, 2 ), 16 ) + "." +
		parseInt( hexstr.substr( 4, 2 ), 16 ) + "." +
		parseInt( hexstr.substr( 2, 2 ), 16 ) + "." +
		parseInt( hexstr.substr( 0, 2 ), 16 );
}

function ContinueSystemFlash( fsrd, flashresponse, pushop )
{
	if( flashresponse[0] == '!' )
	{
		pushop.status_callback( 0, flashresponse, pushop );
		console.log( flashresponse );
		return;
	}

	var cont = pushop.status_callback( 1, flashresponse, pushop );

	if( !cont ) return;
	if( pushop.place >= pushop.padlen ) return;

	//If we are coming from a write, and now we need to erase the next block, do so.

	if( ( pushop.place % flash_blocksize ) == 0 && flashresponse[1] != 'B' )
	{
		QueueOperation( "FB" + ((pushop.place+pushop.base_address)/flash_blocksize),  function( x, y ) { ContinueSystemFlash( x, y, pushop ); } );
	}
	else  	//Done erasing the next block, or coming off a write we don't need to erase?
	{
		var addy = pushop.place + pushop.base_address;
		var sendstr = "FX" + addy + "\t" + flash_sendsize + "\t";
		for( var i = 0; i < flash_sendsize; i++ )
		{
			sendstr += tohex8( pushop.paddata[pushop.place++] );
		}
		QueueOperation( sendstr, function( x, y ) { ContinueSystemFlash( x, y, pushop ); } );
	}
}

//The signature for status callback is: function AVRStatusCallback( is_ok, comment, pushop )
//If pushop.place == pushop.padlen, no further callbacks will continue, even if true is returned.
//you must return "true."  Returning false will cease further pushing.
//This function returns an object with all properties about the transfer.
//WARNING: "location" must be block (65536) aligned.
function PushImageTo( arraydata, location, status_callback, ctx )
{
	if( location & 0xffff != 0 )
	{
		console.log( "Error: to address not 65,536 aligned." );
		return null;
	}

	var pushop = Object();
	pushop.padlen = Math.floor(((arraydata.byteLength-1)/flash_sendsize)+1)*flash_sendsize;
	pushop.paddata = new Uint8Array( pushop.padlen, 0 );
	pushop.paddata.set( new Uint8Array( arraydata ), 0 );
	pushop.status_callback = status_callback;
	pushop.place = 0;
	pushop.base_address = location;
	pushop.ctx = ctx;

	ContinueSystemFlash( null, "Starting", pushop );

	return pushop;
}





/* MD5 implementation minified from: http://blog.faultylabs.com/files/md5.js
 Javascript MD5 library - version 0.4 Coded (2011) by Luigi Galli - LG@4e71.org - http://faultylabs.com
 Thanks to: Roberto Viola  The below code is PUBLIC DOMAIN - NO WARRANTY!
 */
"undefined"==typeof faultylabs&&(faultylabs={}),faultylabs.MD5=function(n){function r(n){var r=(n>>>0).toString(16);return"00000000".substr(0,8-r.length)+r}function t(n){for(var r=[],t=0;t<n.length;t++)r=r.concat(s(n[t]));return r}function e(n){for(var r=[],t=0;8>t;t++)r.push(255&n),n>>>=8;return r}function o(n,r){return n<<r&4294967295|n>>>32-r}function a(n,r,t){return n&r|~n&t}function f(n,r,t){return t&n|~t&r}function u(n,r,t){return n^r^t}function i(n,r,t){return r^(n|~t)}function c(n,r){return n[r+3]<<24|n[r+2]<<16|n[r+1]<<8|n[r]}function s(n){for(var r=[],t=0;t<n.length;t++)if(n.charCodeAt(t)<=127)r.push(n.charCodeAt(t));else for(var e=encodeURIComponent(n.charAt(t)).substr(1).split("%"),o=0;o<e.length;o++)r.push(parseInt(e[o],16));return r}function l(){for(var n="",t=0,e=0,o=3;o>=0;o--)e=arguments[o],t=255&e,e>>>=8,t<<=8,t|=255&e,e>>>=8,t<<=8,t|=255&e,e>>>=8,t<<=8,t|=e,n+=r(t);return n}function y(n){for(var r=new Array(n.length),t=0;t<n.length;t++)r[t]=n[t];return r}function h(n,r){return 4294967295&n+r}function p(){function n(n,r,t,e){var a=m;m=U,U=d,d=h(d,o(h(b,h(n,h(r,t))),e)),b=a}var r=A.length;A.push(128);var t=A.length%64;if(t>56){for(var s=0;64-t>s;s++)A.push(0);t=A.length%64}for(s=0;56-t>s;s++)A.push(0);A=A.concat(e(8*r));var y=1732584193,p=4023233417,g=2562383102,v=271733878,b=0,d=0,U=0,m=0;for(s=0;s<A.length/64;s++){b=y,d=p,U=g,m=v;var I=64*s;n(a(d,U,m),3614090360,c(A,I),7),n(a(d,U,m),3905402710,c(A,I+4),12),n(a(d,U,m),606105819,c(A,I+8),17),n(a(d,U,m),3250441966,c(A,I+12),22),n(a(d,U,m),4118548399,c(A,I+16),7),n(a(d,U,m),1200080426,c(A,I+20),12),n(a(d,U,m),2821735955,c(A,I+24),17),n(a(d,U,m),4249261313,c(A,I+28),22),n(a(d,U,m),1770035416,c(A,I+32),7),n(a(d,U,m),2336552879,c(A,I+36),12),n(a(d,U,m),4294925233,c(A,I+40),17),n(a(d,U,m),2304563134,c(A,I+44),22),n(a(d,U,m),1804603682,c(A,I+48),7),n(a(d,U,m),4254626195,c(A,I+52),12),n(a(d,U,m),2792965006,c(A,I+56),17),n(a(d,U,m),1236535329,c(A,I+60),22),n(f(d,U,m),4129170786,c(A,I+4),5),n(f(d,U,m),3225465664,c(A,I+24),9),n(f(d,U,m),643717713,c(A,I+44),14),n(f(d,U,m),3921069994,c(A,I),20),n(f(d,U,m),3593408605,c(A,I+20),5),n(f(d,U,m),38016083,c(A,I+40),9),n(f(d,U,m),3634488961,c(A,I+60),14),n(f(d,U,m),3889429448,c(A,I+16),20),n(f(d,U,m),568446438,c(A,I+36),5),n(f(d,U,m),3275163606,c(A,I+56),9),n(f(d,U,m),4107603335,c(A,I+12),14),n(f(d,U,m),1163531501,c(A,I+32),20),n(f(d,U,m),2850285829,c(A,I+52),5),n(f(d,U,m),4243563512,c(A,I+8),9),n(f(d,U,m),1735328473,c(A,I+28),14),n(f(d,U,m),2368359562,c(A,I+48),20),n(u(d,U,m),4294588738,c(A,I+20),4),n(u(d,U,m),2272392833,c(A,I+32),11),n(u(d,U,m),1839030562,c(A,I+44),16),n(u(d,U,m),4259657740,c(A,I+56),23),n(u(d,U,m),2763975236,c(A,I+4),4),n(u(d,U,m),1272893353,c(A,I+16),11),n(u(d,U,m),4139469664,c(A,I+28),16),n(u(d,U,m),3200236656,c(A,I+40),23),n(u(d,U,m),681279174,c(A,I+52),4),n(u(d,U,m),3936430074,c(A,I),11),n(u(d,U,m),3572445317,c(A,I+12),16),n(u(d,U,m),76029189,c(A,I+24),23),n(u(d,U,m),3654602809,c(A,I+36),4),n(u(d,U,m),3873151461,c(A,I+48),11),n(u(d,U,m),530742520,c(A,I+60),16),n(u(d,U,m),3299628645,c(A,I+8),23),n(i(d,U,m),4096336452,c(A,I),6),n(i(d,U,m),1126891415,c(A,I+28),10),n(i(d,U,m),2878612391,c(A,I+56),15),n(i(d,U,m),4237533241,c(A,I+20),21),n(i(d,U,m),1700485571,c(A,I+48),6),n(i(d,U,m),2399980690,c(A,I+12),10),n(i(d,U,m),4293915773,c(A,I+40),15),n(i(d,U,m),2240044497,c(A,I+4),21),n(i(d,U,m),1873313359,c(A,I+32),6),n(i(d,U,m),4264355552,c(A,I+60),10),n(i(d,U,m),2734768916,c(A,I+24),15),n(i(d,U,m),1309151649,c(A,I+52),21),n(i(d,U,m),4149444226,c(A,I+16),6),n(i(d,U,m),3174756917,c(A,I+44),10),n(i(d,U,m),718787259,c(A,I+8),15),n(i(d,U,m),3951481745,c(A,I+36),21),y=h(y,b),p=h(p,d),g=h(g,U),v=h(v,m)}return l(v,g,p,y).toUpperCase()}var A=null,g=null;return"string"==typeof n?A=s(n):n.constructor==Array?0===n.length?A=n:"string"==typeof n[0]?A=t(n):"number"==typeof n[0]?A=n:g=typeof n[0]:"undefined"!=typeof ArrayBuffer?n instanceof ArrayBuffer?A=y(new Uint8Array(n)):n instanceof Uint8Array||n instanceof Int8Array?A=y(n):n instanceof Uint32Array||n instanceof Int32Array||n instanceof Uint16Array||n instanceof Int16Array||n instanceof Float32Array||n instanceof Float64Array?A=y(new Uint8Array(n.buffer)):g=typeof n:g=typeof n,g&&alert("MD5 type mismatch, cannot process "+g),p()};


