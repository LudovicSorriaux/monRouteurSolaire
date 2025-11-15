var $ = Dom7;


var app = new Framework7({
  name: 'Ma Cuve Control', // App name
  theme: 'auto', // Automatic theme detection
  el: '#app', // App root element
  
  // App store
  store: store,
  // App routes
  routes: routes,
  on: {
    // each object key means same name event handler
    pageInit: function (page) {
      console.log('Page init : ' +page.name);
      if(page.name === 'home'){
        console.log('Page is home; creating SSE');
        sse = new SSE('/cuveEvents', {
          onOpen: function (e) {
              console.log("Open SSE to /cuveEvents");
              console.log(e);
          },
          onEnd: function (e) {
              console.log("Ending SSE /cuveEvents");
              console.log(e);
          },
          onError: function (e) {
              console.log("Could not connect");
              app.dialog.alert("Could not connect to SSE /cuveEvents try again later",'Erreur !')
          },
          onMessage: function (e) {
              console.log("Message from /cuveEvents");
              console.log(e);
              data = $.trim(e.data);
              if(data.includes('hello!')){
                initPageParams();
              }
          },
          options: {
              forceAjax: false
          },
          events: {
            cuveData: function (e) {
              console.log("Message from /cuveEvents/cuveData");
              console.log(e);
              cuveDataServer();
              }
          },
        });
      }
    },
    pageBeforeIn:function (page) {
      console.log('Page before in is : '+page.name);
      if(page.name === 'home'){
        console.log('Page is home; starting SSE');
        sse.start();
      } 
    },
    pageBeforeOut:function (page) {
      console.log('Page before out is : '+page.name);
      if(page.name === 'home'){
        console.log('Page is home; stopping SSE');
        sse.stop();
      } 
    },
    popupOpened: function (popup) {
      console.log('Before open popup '+ popup.params.name +' Event'); 
      if(popup.params.name === 'deleteUsr'){
        var frm = new FormData();
        validateOnServer('/getUsers',frm)
      }
    },
    popupClose: function (popup) {
      console.log('Popup close : '+popup.params.name)
      if(popup.params.name === 'addUsr'){
        if(wasInLogin) 
          app.loginScreen.open('#my-login-screen'); 
      } else if(popup.params.name === 'EndSession'){
        app.loginScreen.open('#my-login-screen'); 
      } else if(popup.params.name === 'logoff'){
        app.panel.close($('#panel-on-right'));
        app.loginScreen.open('#my-login-screen'); 
      }
    },
  },
});


// init water level to zero; waiting for real update from sse
document.getElementById("waterLevel").style.height = '0%'

// Start with Login sceeen
app.loginScreen.open('#my-login-screen')



// global variables 
var maCuve = maCuve || {};
var today = new Date();
var statusErrorMap = {
  '400' : "Server understood the request, but request content was invalid.",
  '401' : "Unauthorized access.",
  '403' : "Forbidden resource can't be accessed.",
  '405' : "Method Not Allowed",
  '500' : "Internal server error.",
  '503' : "Service unavailable."
};

var sse;
var wasInLogin = false;

var sessID;
var ttl;
var expirationDate = 0;
var userName;
var passWord;
var timeoutPPID;


// maCuve.session sigleton class to handle session infos
  maCuve.Session = (function () {
    var instance;

    function init() {
        var sessionIdKey = "maCuve-session";
        return {
        // Public methods and variables.
        set: function (sessionData) {
          window.localStorage.setItem(sessionIdKey, JSON.stringify(sessionData));
        },

        get: function () {
            var result = null;
            try {
            result = JSON.parse(window.localStorage.getItem(sessionIdKey));
          } catch(e){}
            return result;
        }
      };
    };

    return {
      getInstance: function () {
        if (!instance) {
          instance = init();
        }
        return instance;
      }
    };
    }());

  function storeUserInfos(){
    // Store the user infos and session. 
    today = new Date();
    expirationDate = today.getTime() + (ttl*1000);	// ttl is in sec where setTime is in millis  *1000
    console.log('In store Infos : now is: ' + today.getTime() + 'and expiration is: ' + expirationDate);
    maCuve.Session.getInstance().set({
      username: userName,
      passord: passWord,
      sessionId: sessID,
      theExpirationDate: expirationDate,
    });
  }
  
// init page params via SSE for home page
  function initPageParams(){
    var formData = new FormData();

    formData.append('sess', sessID);
    validateOnServer('/setCuveSSEData', formData);
  }

  // receive SSE message from server to home page
  function cuveDataServer(evt){
    var today = new Date();
    var timeLeft;
    var wordDiv = '';
    var waterIntDiv = ''; 
    var waterExtDiv = ''; 
    var returnedData;

  
    console.log("cuveDataServer");
    console.log(evt);
    if (expirationDate < today.getTime()) {						// check that the ttl is still valid move to login if not. 
      console.log('Session ttl expired: go to login diag');
      console.log('expiration is ' + expirationDate + ' and now is ' + today.getTime());
      app.popup.open($('#EndSession-popup'));
    } else {													// session is still valid so perform data!
      timeLeft = 	(expirationDate - today.getTime())/1000;
      console.log('Session is still valid, time left to run : ' + timeLeft + ' secs');
      /*    {
              "water" : 2000,
              "pct" : 10,
              "manuSW" : true/false,
              "tropPleinSW" : true/false,
              "eauvilleSW" : true/false,
              "moteurSW" : true/false,
              "flgPBTropPlein" : true/false,
            }
      */
      data = $.trim(evt.data);
      returnedData = JSON.parse(data);
      console.log("serverEvent json is " + JSON.stringify(returnedData));

      if(returnedData.hasOwnProperty('water')){
        waterIntDiv += '<span id="waterInt" >'+returnedData.water
        waterExtDiv += '<div id="waterExt" class="waterLevel">'+returnedData.water
      }  
      if(returnedData.hasOwnProperty('pct')){
//        if(typeof returnedData.pct === 'number')
        if(!isNaN(returnedData.pct)){
          document.getElementById("waterLevel").style.height = ''+returnedData.pct+'%'
          if(waterIntDiv.length == 0){  // got not water quantity
            waterIntDiv += '<span id="waterInt" >'+returnedData.pct+'</span>'
            waterExtDiv += '<div id="waterExt" class="waterLevel">'+returnedData.pct+'</div>'
          } else {
            waterIntDiv += ' / >'+returnedData.pct+'</span>'
            waterExtDiv += ' / >'+returnedData.pct+'</div>'
          }  
          if(returnedData.pct >= 15){ // plus de 15% donc inside tank
            $('#waterExt').empty();
            $('#waterInt').empty();
            $('#waterExt').append('<div id="waterExt" class="waterLevel"></div>').trigger( "create" );
            $('#waterInt').append(waterIntDiv).trigger( "create" );

          } else {    // moins de 25% donc outside Tank
            $('#waterExt').empty();
            $('#waterInt').empty();
            $('#waterInt').append('<span id="waterInt" ></span>').trigger( "create" );
            $('#waterExt').append(waterExtDiv).trigger( "create" );
          }
        }
      }
      if(returnedData.hasOwnProperty('manuSW')){
        wordDiv = ''
        if(returnedData.manuSW){    
          if (!$('#manuSwitch').prop('checked')){   // if false need to change
            wordDiv += '<div class="switch" id="manuSW">'
            wordDiv += '<input type="checkbox" name="Manu" id="manuSwitch" checked="true">'
            wordDiv += '<label for="Manu" class="switchLabel switchChecked">'
            wordDiv += '<i class="switchChecked"></i>'
            wordDiv += '</label>'
            wordDiv += '<span class="switchChecked"></span>'
            wordDiv += '</div>'
          }
        } else {    // manuSW = false
          if ($('#manuSwitch').prop('checked')){    // if true need to change
            wordDiv += '<div class="switch" id="manuSW">'
            wordDiv += '<input type="checkbox" name="Manu" id="manuSwitch" checked="false">'
            wordDiv += '<label for="Manu" class="switchLabel switchUnchecked">'
            wordDiv += '<i class="switchUnchecked"></i>'
            wordDiv += '</label>'
            wordDiv += '<span class="switchUnchecked"></span>'
            wordDiv += '</div>'
          }
        }
        $('#manuSW').empty();
        $('#manuSW').append(wordDiv).trigger( "create" );
      }  
      if(returnedData.hasOwnProperty('tropPleinSW')){
        wordDiv = ''
        if(returnedData.tropPlein){    
          if (!$('#troppleinSwitch').prop('checked')){   // if false need to change
            wordDiv += '<div class="switch" id="troppleinSW">'
            wordDiv += '<input type="checkbox" name="tropPlein" id="troppleinSwitch" checked="true">'
            wordDiv += '<label for="tropPlein" class="switchLabel switchChecked">'
            wordDiv += '<i class="switchChecked"></i>'
            wordDiv += '</label>'
            wordDiv += '<span class="switchChecked"></span>'
            wordDiv += '</div>'
          }
        } else {    // manuSW = false
          if ($('#troppleinSwitch').prop('checked')){    // if true need to change
            wordDiv += '<div class="switch" id="troppleinSW">'
            wordDiv += '<input type="checkbox" name="tropPlein" id="troppleinSwitch" checked="false">'
            wordDiv += '<label for="tropPlein" class="switchLabel switchUnchecked">'
            wordDiv += '<i class="switchUnchecked"></i>'
            wordDiv += '</label>'
            wordDiv += '<span class="switchUnchecked"></span>'
            wordDiv += '</div>'
          }
        }
        $('#troppleinSW').empty();
        $('#troppleinSW').append(wordDiv).trigger( "create" );
      }  
      if(returnedData.hasOwnProperty('eauvilleSW')){
        wordDiv = ''
        if(returnedData.eauville){    
          if (!$('#eauvilleSwitch').prop('checked')){   // if false need to change
            wordDiv += '<div class="switch" id="eauvilleSW">'
            wordDiv += '<input type="checkbox" name="eauville" id="eauvilleSwitch" checked="true">'
            wordDiv += '<label for="eauville" class="switchLabel switchChecked">'
            wordDiv += '<i class="switchChecked"></i>'
            wordDiv += '</label>'
            wordDiv += '<span class="switchChecked"></span>'
            wordDiv += '</div>'
          }
        } else {    // manuSW = false
          if ($('#eauvilleSwitch').prop('checked')){    // if true need to change
            wordDiv += '<div class="switch" id="eauvilleSW">'
            wordDiv += '<input type="checkbox" name="eauville" id="eauvilleSwitch" checked="false">'
            wordDiv += '<label for="eauville" class="switchLabel switchUnchecked">'
            wordDiv += '<i class="switchUnchecked"></i>'
            wordDiv += '</label>'
            wordDiv += '<span class="switchUnchecked"></span>'
            wordDiv += '</div>'
          }
        }
        $('#eauvilleSW').empty();
        $('#eauvilleSW').append(wordDiv).trigger( "create" );
      }  
      if(returnedData.hasOwnProperty('moteurSW')){
        wordDiv = ''
        if(returnedData.moteurSW){    
          if (!$('#moteurSwitch').prop('checked')){   // if false need to change
            wordDiv += '<div class="switch" id="moteurSW">'
            wordDiv += '<input type="checkbox" name="moteur" id="moteurSwitch" checked="true">'
            wordDiv += '<label for="moteur" class="switchLabel switchChecked">'
            wordDiv += '<i class="switchChecked"></i>'
            wordDiv += '</label>'
            wordDiv += '<span class="switchChecked"></span>'
            wordDiv += '</div>'
          }
        } else {    // manuSW = false
          if ($('#moteurSwitch').prop('checked')){    // if true need to change
            wordDiv += '<div class="switch" id="moteurSW">'
            wordDiv += '<input type="checkbox" name="moteur" id="moteurSwitch" checked="false">'
            wordDiv += '<label for="moteur" class="switchLabel switchUnchecked">'
            wordDiv += '<i class="switchUnchecked"></i>'
            wordDiv += '</label>'
            wordDiv += '<span class="switchUnchecked"></span>'
            wordDiv += '</div>'
          }
        }
        $('#moteurSW').empty();
        $('#moteurSW').append(wordDiv).trigger( "create" );
      }  
      if(returnedData.hasOwnProperty('flgPBTropPlein')){
        if(returnedData.flgPBTropPlein){ 
          document.getElementById("tropPleinErreur").classList.add("hidden");
        } else {
          document.getElementById("tropPleinErreur").classList.remove("hidden");
        }
      }
       
    }
  }

  // Switches 
  $('#manuSwitch').on('click', function () {
    var formData = new FormData();
    today = new Date();

    if (expirationDate < today.getTime()) {						// check that the ttl is still valid move to login if not. 
      console.log('Session ttl expired: go to login diag');
      console.log('expiration is ' + expirationDate + ' and now is ' + today.getTime());
      app.popup.open($('#EndSession-popup'));
    } else {													// session is still valid so perform data!
      timeLeft = 	(expirationDate - today.getTime())/1000;
      console.log('Session is still valid, time left to run : ' + timeLeft + ' secs');

      formData.append('sess', sessID);
      formData.append('switch','manuSwitch')
      formData.append('value', $('#manuSwitch').prop('checked'))
      validateOnServer('/setSwitches',formData)
    }
  });

  $('#moteurSwitch').on('click', function () {
    var formData = new FormData();
    today = new Date();

    if (expirationDate < today.getTime()) {						// check that the ttl is still valid move to login if not. 
      console.log('Session ttl expired: go to login diag');
      console.log('expiration is ' + expirationDate + ' and now is ' + today.getTime());
      app.popup.open($('#EndSession-popup'));
    } else {													// session is still valid so perform data!
      timeLeft = 	(expirationDate - today.getTime())/1000;
      console.log('Session is still valid, time left to run : ' + timeLeft + ' secs');

      formData.append('sess', sessID);
      formData.append('switch','moteurSwitch')
      formData.append('value', $('#moteurSwitch').prop('checked'))
      validateOnServer('/setSwitches',formData)
    }
  });

  $('#troppleinSwitch').on('click', function () {
    var formData = new FormData();
    today = new Date();

    if (expirationDate < today.getTime()) {						// check that the ttl is still valid move to login if not. 
      console.log('Session ttl expired: go to login diag');
      console.log('expiration is ' + expirationDate + ' and now is ' + today.getTime());
      app.popup.open($('#EndSession-popup'));
    } else {													// session is still valid so perform data!
      timeLeft = 	(expirationDate - today.getTime())/1000;
      console.log('Session is still valid, time left to run : ' + timeLeft + ' secs');

      formData.append('sess', sessID);
      formData.append('switch','troppleinSwitch')
      formData.append('value', $('#troppleinSwitch').prop('checked'))
      validateOnServer('/setSwitches',formData)
    }
  });

  $('#eauvilleSwitch').on('click', function () {
    var formData = new FormData();
    today = new Date();

    if (expirationDate < today.getTime()) {						// check that the ttl is still valid move to login if not. 
      console.log('Session ttl expired: go to login diag');
      console.log('expiration is ' + expirationDate + ' and now is ' + today.getTime());
      app.popup.open($('#EndSession-popup'));
    } else {													// session is still valid so perform data!
      timeLeft = 	(expirationDate - today.getTime())/1000;
      console.log('Session is still valid, time left to run : ' + timeLeft + ' secs');

      formData.append('sess', sessID);
      formData.append('switch','eauvilleSwitch')
      formData.append('value', $('#eauvilleSwitch').prop('checked'))
      validateOnServer('/setSwitches',formData)
    }
  });



  // Validate acction on server
  const validateOnServer = async function(url,formData) {
    var contentType;
    var result;
    var message;
    
    console.log('server call on: '+url+' parameters: '+app.utils.serializeObject(formData));

    try {
      let response = await fetch(url, { method: 'POST',
                                        body: formData,
                                        mode: 'cors',
                                        headers: {'Access-Control-Allow-Origin': '*',},
                                      });   
      if (response.ok) {
        console.log('HTTP status is successful');
        contentType = response.headers.get('Content-Type')
        if (contentType && contentType.includes('application/json')) {
          result = await response.json();
          onSuccess(result);
        } else {
          console.log('Error: Server response not json');
          message = "Error: Server response not json";
          throw new Error(message);
        }
      } else {                    // not a 200 response
        console.log('HTTP status is not successful');
        if (response.status) {    // la reponse est valide
          if(response.status == "400") {
            if (response.responseText.indexOf("Invalid Session") !== -1){				//400: Invalid Session
              console.log('THE SESSIONID EXPIRED NEXT CHANGE PAGE WILL GO TO LOGIN');
              app.popup.open($('#EndSession-popup'));
            } else if(response.responseText.indexOf("Invalid Request") !== -1){   //400: Invalid Request
              console.log("the POST request doesn't have good parameters");
              message="the request doesn't have the requested parameters";
              throw new Error(message);
            }
          } else if (response.status === 500){                        // test if error is json from server
            contentType = response.headers.get('Content-Type')
            if (contentType && contentType.includes('application/json')) {
              result = await response.json();
              onError(result);
            } else {
              message = "Server Error => "+ response.status + ": "+ response.responseText;
              throw new Error(message);
            }
          } else {    // not a 400 or 500 error
            message = "Error => "+ response.status + ": "+ statusErrorMap[response.status];
            if(!message){
              message="Unknown Error\n Response Status: " + response.status + "\n Response Text: " + response.responseText;
            }
            throw new Error(message);
          }
        } else {
          message="Unknown Server Error";
          throw new Error(message);
        }
      }
    } catch (error) {
      console.error('Server Fetch ', error);
      app.dialog.alert(message,'Erreur !');

        /* ===> Debug */ 
      if(app.loginScreen.get($('#my-login-screen')).opened){
        app.loginScreen.close($('#my-login-screen'));   // Close login screen
      }
    }
  }
    function onSuccess(returnedData){
      var message ='';
      var popup;

      console.log("returned json is " + JSON.stringify(returnedData));

      message = "<H2>"+returnedData.status+"</H2>"
      message += "<br/"+returnedData.message

      if(returnedData.status == "Processed"){
        message ='';    // call is sucessfull do nothing !!! 
      } else if (returnedData.status == "Log in Successful"){              // login successful => main.html  
        userName = returnedData.username;	
        passWord = returnedData.password;	
        sessID = returnedData.sessionID;
        ttl = returnedData.ttl;
        storeUserInfos();
        $("#thename").val('');
        $("#thepassword").val('');
        $("#keepAlive").prop('checked', false);
        app.loginScreen.close('#my-login-screen');   // Close login screen
        message += "br/>Your Session Time is : " + ttl + " seconds"
        app.dialog.alert(message,'Information');
      } else if (returnedData.status == "Log in Failed"){           // Login failed => retry
        app.dialog.alert(message,'Erreur !');
      } else if (returnedData.status == "User Already Exist"){      // Registration user exist => retry
        app.dialog.alert(message,'Erreur !');
      } else if (returnedData.status == "New User Created Succesfully"){     // Registration new user => main.html 
        if(returnedData.flgLogin == "true"){
          userName = returnedData.username;	
          passWord = returnedData.password;	
          sessID = returnedData.sessionID;
          ttl = returnedData.ttl;
          storeUserInfos();
          $("#newname").val('');
          $("#newpassword").val('');
          $("#newpassword2").val('');
          $("#adminpassword").val('');
            popup = app.popup.get($('#addUsr-popup'));
          if(popup !== undefined){
            if(popup.opened){
              app.popup.close($('#addUsr-popup'));
            }
          }
          popup = app.panel.get($('#panel-on-right'));
          if(popup !== undefined){
            if(popup.opened){
              app.panel.close($('#panel-on-right'));   // Close right panel
            }
          }
          popup = app.popup.get($('#my-login-screen'));
          if(popup !== undefined){
            if(popup.opened){
              app.loginScreen.close($('#my-login-screen'));   // Close login screen
            }
          }
          message += "br/>Your Session Time is : " + ttl + " seconds"
          app.dialog.alert(message,'Information');
        } else {			// add user but already login 
          app.dialog.alert('User Added but not loged in','Information');
        }  
          // Registration no room for new user => login
      } else if (returnedData.status == "No room for new user"){      
        app.dialog.alert(message,'Erreur !');
      } else if (returnedData.status == "Bad AdminPassword"){      	// Registration bad adminPassword => retry	
        app.dialog.alert(message,'Erreur !');
      } else if (returnedData.status == "Admin Password Updated"){  // change admin password updated => login   
        $("#oldadminpasswd").val('');
        $("#newadminpassword").val('');
        $("#adminpasswordchk").val('');
        popup = app.popup.get($('#adminUsrPasswd-popup'));
        if(popup !== undefined){
          if(popup.opened){
            app.popup.close($('#adminUsrPasswd-popup'));
            app.panel.close($('#panel-on-right'));
          }
        }
        app.dialog.alert(message,'Information');
      } else if (returnedData.status == "Bad Admin Password"){      // change admin password bad adminPassword => retry
        app.dialog.alert(message,'Erreur !');
      } else if (returnedData.status == "User(s) Deleted"){      
        popup = app.popup.get($('#deleteUsr-popup'));
        if(popup !== undefined){
          if(popup.opened){
            app.popup.close($('#deleteUsr-popup'));
            app.panel.close($('#panel-on-right'));
          }
        }
        app.dialog.alert(message,'Information');
      } else if (returnedData.status == "User(s) Listed"){
        if(returnedData.hasOwnProperty('users')){      
          var html = "";
          $.each( returnedData.users, function ( i, obj ) {
            html += constructUserList(obj,i);
          });
          $('#usersList').empty();
          $('#usersList').html( html );
          $('#usersList').trigger('create');
          $('#usersList').listview('refresh');
        }
      } else if (returnedData.status == "No User(s) to Delete"){      
        app.dialog.alert(message,'Erreur !');
      }	
    }
    function onError(result){
      var message;
      /*    {
              "exception" : 'parsererror','timeout','abort'
              "ErrorStatus" : 'The Status',
              "Correction" : 'This what to do ..',
            }
      */
        console.log("JSON Server Error is " + JSON.stringify(result));
        document.getElementById("CuveException").classList.add("hidden");
        document.getElementById("CuveErrorStatus").classList.add("hidden");
        document.getElementById("CuveErrorCorrection").classList.add("hidden");

        if(result.hasOwnProperty('exception')){
          if (result.exception=='parsererror'){
            message="Error.\nParsing JSON Request failed.";
          }else if(result.exception=='timeout'){
            message="Request Time out.";
          }else if(result.exception=='abort'){
            message="Request was aborted by the server";
          }else {
            message="Uncaught Server Error.";
          }
          document.getElementById("CuveException").classList.remove("hidden");
          document.getElementById("CuveException").val(message);
        }
        if(result.hasOwnProperty('ErrorStatus')){
          document.getElementById("CuveErrorStatus").classList.remove("hidden");
          document.getElementById("CuveErrorStatus").val(result.ErrorStatus);
        }
        if(result.hasOwnProperty('Correction')){
          document.getElementById("CuveErrorCorrection").classList.remove("hidden");
          document.getElementById("CuveErrorCorrection").val(result.Correction);
        }
        app.popup.open($('#Erreur-popup'));

        /* ===> Debug */ 
      popup = app.popup.get($('#addUsr-popup'));
      if(popup !== undefined){
        if(popup.opened){
          app.popup.close($('#addUsr-popup'));
          app.panel.close();
        }
      }
      popup = app.popup.get($('#deleteUsr-popup'));
      if(popup !== undefined){
        if(popup.opened){
          app.popup.close($('#deleteUsr-popup'));
          app.panel.close();
        }
      }
      popup = app.popup.get($('#adminUsrPasswd-popup'));
      if(popup !== undefined){
        if(popup.opened){
          app.popup.close($('#adminUsrPasswd-popup'));
          app.panel.close();
        }
      }
      if(app.loginScreen.get($('#my-login-screen')).opened){
        app.loginScreen.close($('#my-login-screen'));   // Close login screen
      }
    }

    // Login 
  $('#my-login-screen .login-button').on('click', function () {
    var valid = false
    var formData;

    valid = app.input.validateInputs($('#my-login-form'));
    if(valid === true){
      formData = app.form.convertToData($('#my-login-form'))
      validateOnServer("/logon",formData);
    } else {
      var text = ''
      if(!app.input.validate($("#thename")))
        text = 'Username invalid'
      if(!app.input.validate($("#thepassword"))){
        if(text.length === 0) 
          text = 'Password invalid'
        else
          text += '<br/>and Password invalid'
      }  
      app.dialog.alert(text,'Erreur !');
      console.log(text);
    }
  });

    // Logoff
  $('#logoffButton').on('click', function () {
    console.log("ask for loging off\n");
    sessID = "";
    userName = "";
    passWord = "";
    expirationDate = new Date();
    maCuve.Session.getInstance().set({										// reset user credentials
      username: "",
      sessionId: "",
      xpirationDate: expirationDate
    });
  });

    // Open add user popup
  $('#addUserButton').on('click', function () {
    console.log("ask for opening add user popup\n");
    wasInLogin = true;
    app.loginScreen.close();
    app.popup.open($('#addUsr-popup'),false);
  });

      // Add New User 
  $('#addUsr').on('click', function () {
    var valid = false
    var formData

    valid = app.input.validateInputs($('#addUsr-form'));
    if(valid === true){
        var password = $("#newpassword").val();
        var password2 = $("#newpassword2").val();
      if(password.localeCompare(password2) != 0){
        var message = 'Password and Password verification are not the same !';
        $("#newpassword").val('');
        $("#newpassword2").val('');
        app.dialog.alert(message);
        console.log(message);
      } else {          // validation is OK so do things
        formData = app.form.convertToData($('#addUsr-form')) 
        validateOnServer("/register",formData);
      }
    } else {            // validation isn't good
      var text = ''
      if(!app.input.validate($("#newname")))
        text = 'Username invalid'
      if(!app.input.validate($("#newpassword"))){
        if(text.length === 0) 
          text = 'Password invalid'
        else
          text += '<br/>and Password invalid'
      }  
      if(!app.input.validate($("#newpassword2"))){
        if(text.length === 0) 
          text = 'Check Password invalid'
        else
          text += '<br/>and Check Password invalid'
      }  
      if(!app.input.validate($("#adminpassword"))){
        if(text.length === 0) 
          text = 'Admin Password invalid'
        else
          text += '<br/>and Admin Password invalid'
      }  
      
      app.dialog.alert(text);
      console.log(text);
    }
  });

    // Change Admin Password 
  $('#adminUsrPasswd').on('click', function () {
    var valid = false;
    var doIt = true;
    var message = '';
    var formData;

    valid = app.input.validateInputs($('#adminUsrPasswd-form'));
    if(valid === true){
      var oldPassword = $("#oldadminpasswd").val();
      var newPassword = $("#newadminpassword").val();
      var checkPassword = $("#adminpasswordchk").val();
      if(oldPassword.localeCompare(newPassword) != 0){
        message += 'new Password and old Password are the same !<br/>';
        $("#oldadminpasswd").val('');
        $("#newadminpassword").val('');
        $("#adminpasswordchk").val('');
        doIt = false;
      }  
      if(newPassword.localeCompare(checkPassword) != 0){
        message += 'new Password and new Password verification are not the same !';
        $("#newadminpassword").val('');
        $("#adminpasswordchk").val('');
        doIt = false;
      }
      if(!doIt){
        app.dialog.alert(message);
        console.log(message);
      } else {          // validation is OK so do things
        formData = app.form.convertToData($('#addUsr-form'))
        validateOnServer("/adminPasswd",formData);
      }
    } else {            // validation isn't good
      if(!app.input.validate($("#adminpassword"))){
        app.dialog.alert('Admin Password invalid');
        console.log('Admin Password invalid');
      }  
      
    }
  });
  
    // Delete User(s) 
  $('#deleteUsr').on('click', function () {
    var valid = false
    var formData

    valid = app.input.validateInputs($('#deleteUsr-form'));
    if(valid === true){
      formData = app.form.convertToData($('#deleteUsr-form'))
      validateOnServer("/deleteUsers",formData);
    } else {            // validation isn't good
      if(!app.input.validate($("#adminpassword"))){
        app.dialog.alert('Admin Password invalid');
        console.log('Admin Password invalid');
      }  
      
    }
  });

    // on page delete users refresh users data from server
  function constructUserList(obj, index){
      var html = "";
      html += '<li>';
      html += '<label class="item-checkbox item-checkbox-icon-start item-content">';
      html += '<input type="checkbox" name="user'+index+'" id="user'+index+'" value="'+obj.username+'" checked="false">';
      html += '<i class="icon icon-checkbox"></i>';
      html += '<div class="item-inner">';
      html += '<div class="item-title">'+obj.username+'"</div>';
      html += '</div>';
      html += '</label>';
      html += '</li>';

      return html;
  }

	





