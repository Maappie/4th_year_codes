class PressesController < ApplicationController
    skip_before_action :verify_authenticity_token

  def index
    @presses = Press.order(created_at: :desc)
  end

  def create
    press = Press.create(
      device_id: params[:device_id],
      pressed_at: params[:pressed_at] || Time.now
    )

    if press.persisted?
      render json: { status: 'ok'}, status: :created
    else
      render json: { status: 'error', errors: press.errors.full_messages}, status: :unprocessable_entity
    end
  end
end
