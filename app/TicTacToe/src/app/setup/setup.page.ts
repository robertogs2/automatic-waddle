import { Component, OnInit } from '@angular/core';
import { AlertController } from '@ionic/angular';
import { Router } from '@angular/router';
import { ConnectionService } from '../connection.service';

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



  constructor(
    private alertCtrl: AlertController,
    private router: Router,
    private connectionServices: ConnectionService) { }
  
  
  onUpdateGameSize(value){
    this.game_size_value = Number(value);
  }

  onUpdateGameType(value){
    this.game_type_value = Number(value);
  }

  onUpdateSymbol(value){
    this.symbol_value = Number(value);
  }


  onConfirm(){
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

    if(this.symbol_value == 0){
      this.connectionServices.setSymbol2(1);
    }
    else if(this.symbol_value == 1){
      this.connectionServices.setSymbol2(2);
    }
    else{
      this.connectionServices.setSymbol2(0);
    }

    console.log("Username: ", this.username);
    console.log("Game Size: ", this.game_size_value);
    console.log("Game Type: ", this.game_type_value);
    console.log("Symbol: ", this.symbol_value);

    this.router.navigate(['/game']);
  }

  ngOnInit() {
  }

}
