import { Component, OnInit } from '@angular/core';
import { AlertController } from '@ionic/angular';
import { Router } from '@angular/router';
import { ConnectionService } from '../connection.service';
import {HttpClient, HttpParams} from '@angular/common/http';

@Component({
  selector: 'app-setup',
  templateUrl: './setup.page.html',
  styleUrls: ['./setup.page.scss'],
})
export class SetupPage implements OnInit {
  symbol_value = 0;
  game_type_value = 0;
  game_size_value = 0;
  username = "";
  dataObject : any;
  response = "a";



  constructor(
    private alertCtrl: AlertController,
    private router: Router,
    private connectionServices: ConnectionService,
    public http: HttpClient) { }
  
  
  onUpdateGameSize(value){
    this.game_size_value = Number(value);
  }

  onUpdateGameType(value){
    this.game_type_value = Number(value);
  }

  onUpdateSymbol(value){
    this.symbol_value = Number(value);
  }

  sendSetUp(){
    const params = new HttpParams().set('username', this.connectionServices.getUser())
                                   .set('size', this.game_size_value.toString())
                                   .set('type', this.game_type_value.toString())
                                   .set('symbol', this.connectionServices.getSymbol1().toString());
    this.http.get('http://' + this.connectionServices.getIP() + ':' + 
    this.connectionServices.getPort() + '/setup', {params}).subscribe((data:any) => {
      console.log(data);
      this.dataObject = data;
      this.response =  this.dataObject['Status'];

      //validate if someone is already waiting
      if (this.response == 'Waiting'){
        this.alertCtrl.create({
          header: 'Error',
          message: 'Another user is waiting. Just User vs User allowed.',
          buttons: [{text: 'Ok', role: 'cancel'}]
        }).then(alertEl => {
          alertEl.present();
          return;
        });
      }

      //validate if someone is already waiting
      else if (this.response == 'Busy'){
        this.alertCtrl.create({
          header: 'Error',
          message: 'Symbol already used, choose another one.',
          buttons: [{text: 'Ok', role: 'cancel'}]
        }).then(alertEl => {
          alertEl.present();
          return;
        });
      }

      //validate if someone is already waiting
      else if (this.response == 'Full'){
        this.alertCtrl.create({
          header: 'Error',
          message: 'Game is full. Wait for the current came to finish.',
          buttons: [{text: 'Ok', role: 'cancel'}]
        }).then(alertEl => {
          alertEl.present();
          return;
        });
      }

      if(this.response == "Ok"){
        this.router.navigate(['/game']);
      }

    });
  }


  onConfirm(){
    //validate entered name
    if (this.username.trim().length <= 0) {
      this.alertCtrl.create({
        header: 'Error',
        message: 'Please enter a username',
        buttons: [{text: 'Ok', role: 'cancel'}]
      }).then(alertEl => {
        alertEl.present();
      });
      return;
    }
    this.connectionServices.setUser(this.username);
    this.connectionServices.setGameSize(this.game_size_value);
    this.connectionServices.setGameType(this.game_type_value);
    this.connectionServices.setSymbol1(this.symbol_value);

    this.sendSetUp();
  }

  ngOnInit() {
  }

}
